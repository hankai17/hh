#!/bin/bash

if [ $# -eq 0 ];then
    echo "usage: $0 inputfile1 [inputfile2 ...]"
    exit 1
fi

for f in "$@"
do
    if [ ! -f "$f" ];then
        echo "skip: $f is not file"
        continue
    fi

    # 找到 "// Generate by hh," 这一行之后的全部内容写入1.cc
    # sed：匹配到pattern之后，输出后面所有行，pattern行本身不输出
    sed -n '/\/\/ Generate by hh,/,$p' "$f" > 1.cc

    # 判断1.cc是否为空（没找到标记）
    if [ ! -s 1.cc ]; then
        echo "[$f] 未找到标记 // Generate by hh, 跳过编译"
        rm -f 1.cc
        continue
    fi

    echo "[$f] 已截取后续内容到 1.cc，开始g++编译"
    g++ 1.cc -o a.out
    if [ $? -eq 0 ];then
        echo "编译成功，生成 a.out"
    else
        echo "g++ 编译失败"
    fi
done

