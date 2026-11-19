/*
 * LeaderboardSystem private, bounded JSON parser/writer (3.1.3).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "json_value.h"
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct json_value {
    json_kind_t kind;
    char *str;                   /* STRING or DECIMAL textual representation */
    uint64_t integer;            /* magnitude for INTEGER */
    bool negative;
    bool boolean;
    json_value_t **items;
    char **keys;                 /* OBJECT only, unique keys */
    size_t count;
};

typedef struct parser {
    const unsigned char *data;
    size_t size, offset;
    int failure;
} parser_t;

typedef struct writer { char *data; size_t size, capacity, limit; } writer_t;

static char *copy_text(const char *s) {
    size_t length = strlen(s);
    char *result = malloc(length + 1U);
    if (result) memcpy(result, s, length + 1U);
    return result;
}
json_value_t *json_new(json_kind_t kind) {
    json_value_t *v = calloc(1U, sizeof(*v));
    if (v) v->kind = kind;
    return v;
}
json_value_t *json_text(const char *s) {
    json_value_t *v = json_new(JSON_STRING);
    if (v) {
        v->str = s ? copy_text(s) : NULL;
        if (!v->str) { json_free(v); return NULL; }
    }
    return v;
}
json_value_t *json_number(uint64_t n) {
    json_value_t *v = json_new(JSON_INTEGER);
    if (v) v->integer = n;
    return v;
}
json_value_t *json_signed(int64_t n) {
    json_value_t *v = json_number(n < 0 ? (uint64_t)(-(n + 1)) + 1U : (uint64_t)n);
    if (v) v->negative = n < 0;
    return v;
}
json_value_t *json_bool(bool b) {
    json_value_t *v = json_new(JSON_BOOLEAN);
    if (v) v->boolean = b;
    return v;
}
void json_free(json_value_t *v) {
    if (!v) return;
    for (size_t i = 0; i < v->count; ++i) {
        json_free(v->items[i]);
        if (v->keys) free(v->keys[i]);
    }
    free(v->items);
    free(v->keys);
    free(v->str);
    free(v);
}
json_kind_t json_kind(const json_value_t *v) { return v ? v->kind : JSON_NULL; }
size_t json_count(const json_value_t *v) { return v ? v->count : 0U; }
const char *json_string(const json_value_t *v) {
    return v && v->kind == JSON_STRING ? v->str : NULL;
}
int json_uint(const json_value_t *v, uint64_t *out) {
    if (!v || v->kind != JSON_INTEGER || v->negative || !out) return -1;
    *out = v->integer;
    return 0;
}
int json_boolean(const json_value_t *v, bool *out) {
    if (!v || v->kind != JSON_BOOLEAN || !out) return -1;
    *out = v->boolean;
    return 0;
}
const json_value_t *json_get(const json_value_t *v, const char *key) {
    if (!v || v->kind != JSON_OBJECT || !key) return NULL;
    for (size_t i = 0; i < v->count; ++i)
        if (strcmp(v->keys[i], key) == 0) return v->items[i];
    return NULL;
}
const json_value_t *json_at(const json_value_t *v, size_t i) {
    return v && v->kind == JSON_ARRAY && i < v->count ? v->items[i] : NULL;
}
bool json_has_only(const json_value_t *v, const char *const *keys, size_t count) {
    if (!v || v->kind != JSON_OBJECT) return false;
    for (size_t i = 0; i < v->count; ++i) {
        bool recognized = false;
        for (size_t j = 0; j < count; ++j)
            if (strcmp(v->keys[i], keys[j]) == 0) { recognized = true; break; }
        if (!recognized) return false;
    }
    return true;
}
static int add_item(json_value_t *v, const char *key, json_value_t *child) {
    if (!v || !child || (v->kind != JSON_ARRAY && v->kind != JSON_OBJECT) ||
        (v->kind == JSON_OBJECT && !key) || v->count >= SIZE_MAX / sizeof(*v->items)) return -1;
    if (v->kind == JSON_OBJECT && json_get(v, key)) return -1;
    /* Separate reallocations are safe because count only advances on success. */
    json_value_t **items = realloc(v->items, (v->count + 1U) * sizeof(*items));
    if (!items) return -1;
    v->items = items;
    if (v->kind == JSON_OBJECT) {
        char **keys = realloc(v->keys, (v->count + 1U) * sizeof(*keys));
        if (!keys) return -1;
        v->keys = keys;
        v->keys[v->count] = copy_text(key);
        if (!v->keys[v->count]) return -1;
    }
    v->items[v->count++] = child;
    return 0;
}
int json_push(json_value_t *v, json_value_t *child) { return add_item(v, NULL, child); }
int json_put(json_value_t *v, const char *key, json_value_t *child) { return add_item(v, key, child); }

static void skip_ws(parser_t *p) {
    while (p->offset < p->size && (p->data[p->offset] == ' ' ||
           p->data[p->offset] == '\n' || p->data[p->offset] == '\r' ||
           p->data[p->offset] == '\t')) ++p->offset;
}
static int utf8_codepoint(const unsigned char *s, size_t n, size_t *used) {
    if (!n) return -1;
    unsigned c = s[0];
    if (c < 128U) { *used=1; return c ? (int)c : -1; }
    unsigned cp, min, extra;
    if (c >= 0xC2U && c <= 0xDFU) { cp = c & 31U; min=128U; extra=1U; }
    else if (c >= 0xE0U && c <= 0xEFU) { cp=c & 15U; min=2048U; extra=2U; }
    else if (c >= 0xF0U && c <= 0xF4U) { cp=c & 7U; min=65536U; extra=3U; }
    else return -1;
    if (n < extra+1U) return -1;
    for (unsigned i=1; i<=extra; ++i) {
        if ((s[i] & 0xC0U) != 0x80U) return -1;
        cp = (cp << 6) | (s[i] & 63U);
    }
    if (cp < min || cp > 0x10FFFFU || (cp >= 0xD800U && cp <= 0xDFFFU)) return -1;
    *used=extra+1U;
    return (int)cp;
}
static int hex_digit(unsigned char c) {
    if (c >= '0' && c <= '9') return c-'0';
    if (c >= 'a' && c <= 'f') return c-'a'+10;
    if (c >= 'A' && c <= 'F') return c-'A'+10;
    return -1;
}
static int parse_hex(parser_t *p, unsigned *out) {
    if (p->size - p->offset < 4U) return -1;
    unsigned cp=0;
    for (unsigned i=0;i<4;++i) {
        int d=hex_digit(p->data[p->offset++]);
        if (d<0) return -1;
        cp=(cp<<4)|(unsigned)d;
    }
    *out=cp;
    return 0;
}
static int append_cp(char *dst, size_t *offset, unsigned cp) {
    if (cp == 0U || cp > 0x10FFFFU || (cp >= 0xD800U && cp <= 0xDFFFU)) return -1;
    if (cp < 128U) dst[(*offset)++]=(char)cp;
    else if (cp < 2048U) { dst[(*offset)++]=(char)(0xC0U|(cp>>6)); dst[(*offset)++]=(char)(0x80U|(cp&63U)); }
    else if (cp < 65536U) { dst[(*offset)++]=(char)(0xE0U|(cp>>12)); dst[(*offset)++]=(char)(0x80U|((cp>>6)&63U)); dst[(*offset)++]=(char)(0x80U|(cp&63U)); }
    else { dst[(*offset)++]=(char)(0xF0U|(cp>>18)); dst[(*offset)++]=(char)(0x80U|((cp>>12)&63U)); dst[(*offset)++]=(char)(0x80U|((cp>>6)&63U)); dst[(*offset)++]=(char)(0x80U|(cp&63U)); }
    return 0;
}
static char *parse_string(parser_t *p) {
    if (p->offset >= p->size || p->data[p->offset++] != '"') return NULL;
    /* Each escape can expand at most to its source width; allocate input tail + 1. */
    char *s = malloc(p->size - p->offset + 1U);
    if (!s) { p->failure=-2; return NULL; }
    size_t out=0;
    while (p->offset < p->size) {
        unsigned char c=p->data[p->offset++];
        if (c == '"') { s[out]='\0'; return s; }
        if (c < 32U) break;
        if (c == '\\') {
            if (p->offset == p->size) break;
            c=p->data[p->offset++];
            switch (c) {
                case '"': case '\\': case '/': s[out++]=(char)c; break;
                case 'b': s[out++]='\b'; break;
                case 'f': s[out++]='\f'; break;
                case 'n': s[out++]='\n'; break;
                case 'r': s[out++]='\r'; break;
                case 't': s[out++]='\t'; break;
                case 'u': {
                    unsigned cp;
                    if (parse_hex(p,&cp)) goto fail;
                    if (cp >= 0xD800U && cp <= 0xDBFFU) {
                        if (p->size-p->offset < 6U || p->data[p->offset++]!='\\' || p->data[p->offset++]!='u') goto fail;
                        unsigned low;
                        if (parse_hex(p,&low) || low < 0xDC00U || low > 0xDFFFU) goto fail;
                        cp=0x10000U+((cp-0xD800U)<<10)+(low-0xDC00U);
                    }
                    if (append_cp(s,&out,cp)) goto fail;
                    break;
                }
                default: goto fail;
            }
        } else if (c >= 128U) {
            --p->offset;
            size_t used;
            if (utf8_codepoint(p->data+p->offset,p->size-p->offset,&used)<0) goto fail;
            memcpy(s+out,p->data+p->offset,used);
            p->offset+=used;out+=used;
        } else s[out++]=(char)c;
    }
fail:
    free(s);
    return NULL;
}
static json_value_t *parse_value(parser_t *p, unsigned depth);
static json_value_t *parse_array(parser_t *p, unsigned depth) {
    json_value_t *v=json_new(JSON_ARRAY);
    if (!v) {p->failure=-2;return NULL;}
    ++p->offset; skip_ws(p);
    if (p->offset < p->size && p->data[p->offset]==']') {++p->offset;return v;}
    for (;;) {
        json_value_t *child=parse_value(p,depth+1U);
        if (!child) goto fail;
        if (json_push(v,child)) {json_free(child);p->failure=-2;goto fail;}
        skip_ws(p);
        if (p->offset>=p->size) goto fail;
        unsigned char c=p->data[p->offset++];
        if (c==']') return v;
        if (c!=',') goto fail;
        skip_ws(p);
    }
fail:json_free(v);return NULL;
}
static json_value_t *parse_object(parser_t *p, unsigned depth) {
    json_value_t *v=json_new(JSON_OBJECT);
    if (!v) {p->failure=-2;return NULL;}
    ++p->offset;skip_ws(p);
    if (p->offset < p->size && p->data[p->offset]=='}') {++p->offset;return v;}
    for (;;) {
        char *key=parse_string(p);
        if (!key) goto fail;
        skip_ws(p);
        if (p->offset>=p->size || p->data[p->offset++]!=':') {free(key);goto fail;}
        json_value_t *child=parse_value(p,depth+1U);
        if (!child) {free(key);goto fail;}
        if (json_get(v,key)) {free(key);json_free(child);goto fail;}
        if (json_put(v,key,child)) {free(key);json_free(child);p->failure=-2;goto fail;}
        free(key);
        skip_ws(p);
        if (p->offset>=p->size) goto fail;
        unsigned char c=p->data[p->offset++];
        if (c=='}') return v;
        if (c!=',') goto fail;
        skip_ws(p);
    }
fail:json_free(v);return NULL;
}
static json_value_t *parse_number(parser_t *p) {
    size_t start=p->offset;
    if (p->data[p->offset]=='-') ++p->offset;
    if (p->offset>=p->size) return NULL;
    if (p->data[p->offset]=='0') ++p->offset;
    else if (p->data[p->offset]>='1' && p->data[p->offset]<='9') {
        do {++p->offset;} while(p->offset<p->size && p->data[p->offset]>='0' && p->data[p->offset]<='9');
    } else return NULL;
    bool decimal=false;
    if (p->offset<p->size && p->data[p->offset]=='.') {
        decimal=true; ++p->offset;
        if (p->offset>=p->size || p->data[p->offset]<'0' || p->data[p->offset]>'9') return NULL;
        do {++p->offset;} while(p->offset<p->size && p->data[p->offset]>='0' && p->data[p->offset]<='9');
    }
    if (p->offset<p->size && (p->data[p->offset]=='e' || p->data[p->offset]=='E')) {
        decimal=true;++p->offset;
        if (p->offset<p->size && (p->data[p->offset]=='+' || p->data[p->offset]=='-')) ++p->offset;
        if (p->offset>=p->size || p->data[p->offset]<'0' || p->data[p->offset]>'9') return NULL;
        do {++p->offset;} while(p->offset<p->size && p->data[p->offset]>='0' && p->data[p->offset]<='9');
    }
    json_value_t *v=json_new(decimal?JSON_DECIMAL:JSON_INTEGER);
    if (!v) {p->failure=-2;return NULL;}
    v->negative=p->data[start]=='-';
    if (decimal) {
        size_t length=p->offset-start;
        v->str=malloc(length+1U);
        if (!v->str) {p->failure=-2;json_free(v);return NULL;}
        memcpy(v->str,p->data+start,length);v->str[length]='\0';
    } else {
        size_t i=start+(v->negative?1U:0U);
        uint64_t mag=0U;
        bool overflow=false;
        for (; i<p->offset; ++i) {
            unsigned digit=p->data[i]-'0';
            if (mag>(UINT64_MAX-digit)/10U) {overflow=true;break;}
            mag=mag*10U+digit;
        }
        if(overflow) {
            /* Valid JSON, but outside our bounded integer representation.
             * Leave it typed as non-integral for semantic validation. */
            size_t length=p->offset-start;
            v->kind=JSON_DECIMAL;
            v->str=malloc(length+1U);
            if(!v->str) {p->failure=-2;json_free(v);return NULL;}
            memcpy(v->str,p->data+start,length);v->str[length]='\0';
        } else v->integer=mag;
    }
    return v;
}
static json_value_t *parse_value(parser_t *p, unsigned depth) {
    skip_ws(p);
    if (p->offset>=p->size || depth>64U) return NULL;
    unsigned char c=p->data[p->offset];
    if (c=='{') return parse_object(p,depth);
    if (c=='[') return parse_array(p,depth);
    if (c=='"') {
        char *s=parse_string(p);
        if (!s) return NULL;
        json_value_t *v=json_new(JSON_STRING);
        if (!v) {p->failure=-2;free(s);return NULL;}
        v->str=s;return v;
    }
    if (c=='-' || (c>='0' && c<='9')) return parse_number(p);
    static const struct {const char *word; json_kind_t kind; bool value;} lit[] = {
        {"true",JSON_BOOLEAN,true},{"false",JSON_BOOLEAN,false},{"null",JSON_NULL,false}
    };
    for (size_t i=0;i<sizeof(lit)/sizeof(lit[0]);++i) {
        size_t n=strlen(lit[i].word);
        if (n<=p->size-p->offset && memcmp(p->data+p->offset,lit[i].word,n)==0) {
            p->offset+=n;
            json_value_t *v=json_new(lit[i].kind);
            if (!v) p->failure=-2;
            else v->boolean=lit[i].value;
            return v;
        }
    }
    return NULL;
}
int json_parse(const char *input, size_t n, json_value_t **out) {
    if (!out) return -1;
    *out=NULL;
    if (!input) return -1;
    parser_t p={(const unsigned char *)input,n,0U,-1};
    json_value_t *v=parse_value(&p,0U);
    if (!v) return p.failure;
    skip_ws(&p);
    if (p.offset!=p.size) {json_free(v);return -1;}
    *out=v;
    return 0;
}

static int write_bytes(writer_t *w, const char *data, size_t n) {
    if (n>w->limit-w->size) return -1;
    size_t required=w->size+n+1U;
    if (required>w->capacity) {
        size_t grow=w->capacity?w->capacity:128U;
        while(grow<required) {
            if (grow>w->limit/2U) {grow=w->limit+1U;break;}
            grow*=2U;
        }
        if (grow<required) grow=required;
        char *mem=realloc(w->data,grow);
        if (!mem) return -1;
        w->data=mem;w->capacity=grow;
    }
    memcpy(w->data+w->size,data,n);w->size+=n;w->data[w->size]='\0';return 0;
}
static int write_string(writer_t *w, const char *s) {
    if (write_bytes(w,"\"",1U)) return -1;
    for (const unsigned char *p=(const unsigned char *)s; *p; ++p) {
        if (*p=='"' || *p=='\\') {
            char escaped[2]={'\\',(char)*p};
            if(write_bytes(w,escaped,2U))return -1;
        } else if (*p<32U) {
            char escaped[7];
            snprintf(escaped,sizeof(escaped),"\\u%04x",(unsigned)*p);
            if(write_bytes(w,escaped,6U))return -1;
        } else if(write_bytes(w,(const char *)p,1U))return -1;
    }
    return write_bytes(w,"\"",1U);
}
static int write_value(writer_t *w, const json_value_t *v, unsigned depth) {
    if(!v || depth>64U)return -1;
    switch(v->kind) {
        case JSON_NULL: return write_bytes(w,"null",4U);
        case JSON_BOOLEAN: return write_bytes(w,v->boolean?"true":"false",v->boolean?4U:5U);
        case JSON_INTEGER: {
            char n[32];int k=snprintf(n,sizeof(n),"%s%" PRIu64,v->negative?"-":"",v->integer);
            return k<0 || (size_t)k>=sizeof(n) ? -1:write_bytes(w,n,(size_t)k);
        }
        case JSON_DECIMAL:return write_bytes(w,v->str,strlen(v->str));
        case JSON_STRING:return write_string(w,v->str);
        case JSON_ARRAY: case JSON_OBJECT: {
            bool obj=v->kind==JSON_OBJECT;
            if(write_bytes(w,obj?"{":"[",1U))return -1;
            for(size_t i=0;i<v->count;++i) {
                if(i && write_bytes(w,",",1U))return -1;
                if(obj && (write_string(w,v->keys[i]) || write_bytes(w,":",1U)))return -1;
                if(write_value(w,v->items[i],depth+1U))return -1;
            }
            return write_bytes(w,obj?"}":"]",1U);
        }
    }
    return -1;
}
char *json_serialize(const json_value_t *v, size_t limit) {
    if(limit==0U || limit==SIZE_MAX)return NULL;
    writer_t w={NULL,0U,0U,limit};
    if (write_value(&w,v,0U)) {free(w.data);return NULL;}
    return w.data;
}
