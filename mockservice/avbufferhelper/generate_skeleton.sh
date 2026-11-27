#!/bin/bash
#* ******************************************************************************
#*  If not stated otherwise in this file or this component's LICENSE
#*  file the following copyright and licenses apply:
#*
#*  Copyright 2025 RDK Management
#*
#*  Licensed under the Apache License, Version 2.0 (the License);
#*  you may not use this file except in compliance with the License.
#*  You may obtain a copy of the License at
#*
#*  http://www.apache.org/licenses/LICENSE-2.0
#*
#*  Unless required by applicable law or agreed to in writing, software
#*  distributed under the License is distributed on an AS IS BASIS,
#*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#*  See the License for the specific language governing permissions and
#*  limitations under the License.
#*
#* ******************************************************************************

HEADER_FILE="$1"
OUTPUT_CPP="$2"

if [[ ! -f "$HEADER_FILE" ]]; then
  echo "Header file not found!"
  exit 1
fi

CLASS_NAME=$(grep -Po 'class\s+\K\w+' "$HEADER_FILE" | head -1)
SKELETON_CLASS="${CLASS_NAME}Skeleton"
HEADER_BASENAME=$(basename "$HEADER_FILE")

echo "#include \"$HEADER_BASENAME\"" > "$OUTPUT_CPP"
echo "#include <iostream>" >> "$OUTPUT_CPP"
echo "" >> "$OUTPUT_CPP"
echo "class $SKELETON_CLASS final : public $CLASS_NAME {" >> "$OUTPUT_CPP"
echo "public:" >> "$OUTPUT_CPP"

# Process each pure virtual method line
grep -E 'virtual .*=\s*0\s*;' "$HEADER_FILE" | while read -r line; do
    # Clean line from comments and spaces
    clean_line=$(echo "$line" | sed -E 's/\/\/.*//' | tr -d '\r')

    # Remove "virtual" and "= 0;"
    sig=$(echo "$clean_line" | sed -E 's/virtual\s+//' | sed -E 's/= *0 *; *//')

    # Extract function name
    func_name=$(echo "$sig" | grep -oP '\w+(?=\s*\()')

    # Extract return type (everything before function name)
    ret_type=$(echo "$sig" | sed -E "s/\s*$func_name\s*\(.*//" | xargs)

    # Extract argument list
    args=$(echo "$sig" | grep -oP '\(.*\)')

    # Set default return value
    case "$ret_type" in
        bool) ret_stmt="return false;" ;;
        void) ret_stmt="" ;;
        int|uint32_t|size_t) ret_stmt="return 0;" ;;
        float|double) ret_stmt="return 0.0;" ;;
        *\*) ret_stmt="return nullptr;" ;;
        *) ret_stmt="return {};" ;;
    esac

    echo "    $ret_type $func_name$args override {" >> "$OUTPUT_CPP"
    echo "        std::cout << \"$func_name called\" << std::endl;" >> "$OUTPUT_CPP"
    [[ -n "$ret_stmt" ]] && echo "        $ret_stmt" >> "$OUTPUT_CPP"
    echo "    }" >> "$OUTPUT_CPP"
    echo "" >> "$OUTPUT_CPP"
done

echo "};" >> "$OUTPUT_CPP"
echo "" >> "$OUTPUT_CPP"

# Factory functions. From AV Buffer 0.2.1.0 the helper is a reference-counted singleton:
#   IAVBufferHelper* getAVBufferHelperInstance();               acquire (refcount++)
#   void releaseAVBufferHelperInstance(IAVBufferHelper*);       release (refcount--, destroy at zero)
# Older headers only declare the getter; the singleton is then simply never destroyed.
cat >> "$OUTPUT_CPP" <<EOF
#include <mutex>

static std::mutex g${SKELETON_CLASS}Mutex;
static ${SKELETON_CLASS}* g${SKELETON_CLASS}Instance = nullptr;
static unsigned int g${SKELETON_CLASS}RefCount = 0;

EOF

grep -E "^\s*[^/].*\*\s*get.*\(\)\s*;" "$HEADER_FILE" | grep -v 'virtual' | while read -r line; do
    ret_type=$(echo "$line" | sed -E 's/\*.*//g' | xargs)
    func_name=$(echo "$line" | grep -oP '\w+(?=\s*\()')
    cat >> "$OUTPUT_CPP" <<EOF
$ret_type* $func_name() {
    std::lock_guard<std::mutex> lock(g${SKELETON_CLASS}Mutex);
    if (g${SKELETON_CLASS}Instance == nullptr) {
        g${SKELETON_CLASS}Instance = new $SKELETON_CLASS();
    }
    ++g${SKELETON_CLASS}RefCount;
    return g${SKELETON_CLASS}Instance;
}

EOF
done

grep -E "^\s*void\s+release\w*\s*\(\s*$CLASS_NAME\s*\*\s*\w*\s*\)\s*;" "$HEADER_FILE" | while read -r line; do
    func_name=$(echo "$line" | grep -oP '\w+(?=\s*\()')
    cat >> "$OUTPUT_CPP" <<EOF
void $func_name($CLASS_NAME* instance) {
    std::lock_guard<std::mutex> lock(g${SKELETON_CLASS}Mutex);
    if (instance == nullptr || instance != g${SKELETON_CLASS}Instance || g${SKELETON_CLASS}RefCount == 0) {
        return;
    }
    if (--g${SKELETON_CLASS}RefCount == 0) {
        delete g${SKELETON_CLASS}Instance;
        g${SKELETON_CLASS}Instance = nullptr;
    }
}

EOF
done
