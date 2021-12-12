#include <math.h>
#include <stdio.h>

#include <iostream>
#include <cmath>

int main(int argc, char**argv) {
  constexpr int matrix_height = 5;
  constexpr float dynamicRange = 40.0;
  constexpr float linearBlend = 0.3;
  constexpr float maxLevel = 0.4;

  float thresholdVertical[matrix_height] = {};
  unsigned int y;
  float n, logLevel, linearLevel;

  for (y=0; y < matrix_height; y++) {
    n = (float)y / (float)(matrix_height - 1);
    logLevel = exp10f(n * -1.0 * (dynamicRange / 20.0));
    linearLevel = 1.0 - n;
    linearLevel = linearLevel * linearBlend;
    logLevel = logLevel * (1.0 - linearBlend);
    thresholdVertical[y] = (logLevel + linearLevel) * maxLevel;
  }

  for (int i = 0; i < matrix_height; i++) {
    printf("i=%d l=%f\n", i, thresholdVertical[i]);
  }
  return 0;
}
