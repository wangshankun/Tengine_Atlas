#ifndef OPS_H
#define OPS_H

void argmax(const float *srcData, float *dstData, int outside, int channel, int n, int threadNum);
void softmax(const float *srcData, float *dstData, int outside, int channel, int threadNum);
void to_nchw(float* src, float* dst, int n, int c, int w, int threadNum);

#endif
