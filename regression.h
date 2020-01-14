#include <stdint.h>

void MakeRegressionMatrix(uint32_t width, uint32_t height, uint32_t n, double* data);

/*

  double y[] = {1,2,3,4};
  
  int32_t n = sizeof(y) / sizeof(*y);
  double Ak;
  double Ab;
  double Bk;
  double Bb;

  countLinearMatrix(n, Ak, Ab, Bk, Bb);

  double a = 0;
  double b = 0;

  for (auto k = 0; k < n; ++k) {
    a += y[k] * (Ab + Ak * k);
    b += y[k] * (Bb + Bk * k);
  }

  std::cout << a << ", " << b << "\n";

  // a /= s ;

  y = a * x + b;

*/

void countLinearMatrix(int32_t n, double& Ak, double& Ab, double& Bk, double& Bb);
