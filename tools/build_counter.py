Import("env")

from pathlib import Path

project_dir = Path(env.subst("$PROJECT_DIR"))
counter_file = project_dir / "build.counter"
revision_header = project_dir / "include" / "HastigBuildRevision.h"
counter = int(counter_file.read_text(encoding="ascii").strip())

if counter < 0:
    raise ValueError(f"Invalid build counter: {counter}")

header_contents = (
    "#pragma once\n\n"
    f'static constexpr const char* HASTIG_AI_REVISION = "v{counter}";\n'
)
if not revision_header.exists() or revision_header.read_text(encoding="ascii") != header_contents:
    revision_header.write_text(header_contents, encoding="ascii")

counter_file.write_text(f"{counter + 1}\n", encoding="ascii")
print(f"Building Hastig revision v{counter}")
