echo "Use -m32 switch to force 32-bit build"
g++ %* -std=c++20 -DNDEBUG -O2 -o bin/lists.exe  src/issues.cpp src/status.cpp src/sections.cpp src/mailing_info.cpp src/report_generator.cpp src/metadata.cpp src/lists.cpp
g++ %* -std=c++20 -o bin/section_data.exe src/section_data.cpp
g++ %* -std=c++20 -o bin/toc_diff.exe src/toc_diff.cpp
g++ %* -std=c++20 -DNDEBUG -O2 -o bin/list_issues.exe src/issues.cpp src/status.cpp src/sections.cpp src/metadata.cpp src/list_issues.cpp
g++ %* -std=c++20 -DNDEBUG -O2 -o bin/set_status.exe  src/set_status.cpp src/status.cpp

