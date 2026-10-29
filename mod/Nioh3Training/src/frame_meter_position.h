#ifndef NIOH3_FRAME_METER_POSITION_H
#define NIOH3_FRAME_METER_POSITION_H
/* Client-relative placement. Automatic mode keeps the whole meter above the
   lower player HUD; extra rows grow upward. Explicit Y remains a top percent. */
static int FmPositionY(int height, int meterHeight, int percent) {
    int y = percent < 0 ? height * 82 / 100 - height * 8 / 1080 - meterHeight
                        : height * percent / 100;
    int maxY = height > meterHeight ? height - meterHeight : 0;
    if (y < 0) y = 0;
    if (y > maxY) y = maxY;
    return y;
}
/* Keep the complete strip (labels and padding included) in the central 60%.
   Integer cell widths round inward; the transparent overlay itself stays full-client. */
static int FmPositionX(int width) { (void)width; return 0; }
static int FmCellWidth(int width,int gutter,int pad,int cells) {
    int available=width*60/100-gutter-pad*2;
    int cw=cells>0?available/cells:1;
    return cw>0?cw:1;
}
#endif
