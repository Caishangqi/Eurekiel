#pragma once

struct Rgba8
{
    unsigned char r = 255;
    unsigned char g = 255;
    unsigned char b = 255;
    unsigned char a = 255;
    Rgba8();
    // 构造函数Rgba8被声明为explicit，这意味着在创建Rgba8对象时，必须显式调用该构造函数，如下所示：
    // Rgba8 color(255, 0, 0, 255); 显式调用构造函数

    // 如果没有使用explicit关键字声明构造函数，那么可以通过隐式转换或复制初始化来创建对象，如下所示：
    // Rgba8 color = Rgba8(255, 0, 0, 255); 隐式转换或复制初始化
    explicit Rgba8(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);
    Rgba8(const Rgba8& copyFrom);
};
