# Migration to LeaderboardSystem 3.0.11

- Source/ABI BREAK: replace `player.h` with `team.h`, and `player_t` with
  `team_t`; `team_create(uuid,name,total)` deep-copies names and stores binary
  UUID identity. `team_destroy` frees the name and allocation. No
  `player_add_championship_points` or other player functions remain.
- Store simulated championship points in the new `standings_t`, **not**
  in immutable historical team data. Create from a competition snapshot,
  record results with `standings_record_match()` (or individually with
  `standings_record_result()`), and reset between simulations. Results are
  supplied by the caller; this milestone does not calculate match outcomes.
- Construct a validated immutable competition with `competition_create`,
  using a complete original fixture list and explicit score availability
  for every team and relevant Serie A matchday.
- Apply `competition_is_simulation_ready` before a full-season simulation;
  pending scores are not finalized zeros.
- The existing generic `ranking_t` contract and public target
  `Leaderboard::leaderboard` are unchanged; rebuild downstream binaries.
- No calendar permutations or goal-scoring rules are implemented yet.
