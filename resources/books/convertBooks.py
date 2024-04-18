#! /usr/bin/env nix-shell
#! nix-shell -i python3 -p python3 python3Packages.unidecode
import os
import shutil
from unidecode import unidecode
import hashlib

def remove_special_characters(text):
    return unidecode(text)

def split_and_store(file_path, chunk_size, output_file, output_path):
    with open(file_path, 'r') as file:
        content = file.read()

    content_clean = remove_special_characters(content)

    content_clean = content_clean.replace('\n', ' ')
    content_clean = content_clean.replace('"', ' ')
    content_clean = content_clean.replace('„', '<')
    content_clean = content_clean.replace('”', '>')

    content_clean = content_clean.replace("     ", ' ')
    content_clean = content_clean.replace("    ", ' ')
    content_clean = content_clean.replace("   ", ' ')
    content_clean = content_clean.replace("  ", ' ')

    # Calculate SHA1 hash
    sha1 = hashlib.sha1(content_clean.encode()).hexdigest()

    # Use only the first 8 characters of the hash
    hash_prefix = sha1[:8]

    chunks = [content_clean[i:i + chunk_size] for i in range(0, len(content_clean), chunk_size)]

    cpp_list = "const char bookList[BOOK_PAGES][" + str(chunk_size + 1) + "] = {"
    cpp_list += ", ".join(f'"{chunk}"' for chunk in chunks)
    cpp_list += "};"
    cpp_list_count = f"#define BOOK_PAGES {len(chunks)}\n"
    
    # Save hash in the header file
    cpp_list_hash = f"#define BOOK_HASH \"{hash_prefix}\"\n"

    with open(output_file, 'w') as file:
        file.write("#ifndef BOOK_H\n")
        file.write("#define BOOK_H\n\n")
        file.write(cpp_list_count + "\n")
        file.write(cpp_list_hash + "\n")
        file.write(cpp_list + "\n\n")
        file.write("#endif // BOOK_H\n")

    if os.path.exists(output_path + output_file):
        os.remove(output_path + output_file)
    shutil.move(output_file, output_path)



file_path = 'book.txt'
output_file_path = 'book.h'
output_path = '../../src/defines/' 
if os.path.exists(file_path):
    split_and_store(file_path, 159, output_file_path, output_path)
    print(f"Newlines removed in {file_path}. book saved to {output_path}{output_file_path}.")
else:
    print(file_path + " doesn't exist - skipping book generation")
