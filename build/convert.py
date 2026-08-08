import re
import sys

def process_single_line(line: str) -> str | None:
    # 去掉行首空白后以 -- 开头：返回 None 代表删除该行
    #stripped = line.lstrip()
    #if stripped.startswith("//"):
        #return None
    line = line.replace("::=", "=")
    # 尖括号标签替换 <a b> → a_b
    pat = r"<([^>]+)>"
    line = re.sub(pat, lambda m: m.group(1).replace(" ", "_"), line)
    # ::= 替换为 =
    return line

def main():
    if len(sys.argv) != 2:
        print("使用方法：python convert.py input.txt")
        sys.exit(1)

    src_path = sys.argv[1]
    dst_path = src_path + ".new"

    new_lines = []
    with open(src_path, "r", encoding="utf-8") as f_in:
        for raw_line in f_in:
            processed = process_single_line(raw_line)
            if processed is not None:
                new_lines.append(processed)

    # 写入新文件
    with open(dst_path, "w", encoding="utf-8") as f_out:
        f_out.writelines(new_lines)

    print(f"处理完成，新文件：{dst_path}")

if __name__ == "__main__":
    main()
