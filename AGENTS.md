# Repository instructions

- Whenever code changes are made, increase `HASTIG_AI_REVISION` in
  `include/HastigBuildRevision.h`. Retain revision updates made by builds.
- Keep `build.counter` greater than `HASTIG_AI_REVISION` so the build script
  continues advancing the revision. If no build updates the revision, update
  the header and counter directly.
