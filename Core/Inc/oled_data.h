#ifndef __OLED_DATA_H
#define __OLED_DATA_H

#include <stdint.h>

/* UTF-8 is used by the GCC/CMake project. Keep the width macro for the
 * V1.2 OLED_ShowChinese compatibility function. */
#define OLED_CHARSET_UTF8
#define OLED_CHN_CHAR_WIDTH          3

/*字模基本单元*/
typedef struct 
{
	char Index[5];						//UTF-8索引，最多4字节加结尾0
	uint8_t Data[32];						//字模数据
} ChineseCell_t;

/*ASCII字模数据声明*/
extern const uint8_t OLED_F8x16[][16];
extern const uint8_t OLED_F6x8[][6];

/*汉字字模数据声明*/
extern const ChineseCell_t OLED_CF16x16[];

/*图像数据声明*/
extern const uint8_t Diode[];
/*按照上面的格式，在这个位置加入新的图像数据声明*/
//...

#endif


/*****************江协科技|版权所有****************/
/*****************jiangxiekeji.com*****************/

