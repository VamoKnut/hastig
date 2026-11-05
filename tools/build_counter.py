Import("env")

from pathlib import Path
import re

project_dir = Path(env.subst("$PROJECT_DIR"))
counter_file = project_dir / "build.counter"
revision_header = project_dir / "include" / "HastigBuildRevision.h"
counter = int(counter_file.read_text(encoding="ascii").strip())

if counter < 0:
    raise ValueError(f"Invalid build counter: {counter}")

header_contents = revision_header.read_text(encoding="ascii")
updated_header, replacements = re.subn(
    r"(HASTIG_AI_REVISION\s*=\s*)\d+u;",
    rf"\g<1>{counter}u;",
    header_contents,
)
if replacements != 1:
    raise ValueError(f"Expected one numeric HASTIG_AI_REVISION in {revision_header}")
if updated_header != header_contents:
    revision_header.write_text(updated_header, encoding="ascii")

counter_file.write_text(f"{counter + 1}\n", encoding="ascii")
print(f"Building Hastig revision {counter}")
