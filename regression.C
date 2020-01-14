#include "regression.h"

#include <stdio.h>

void ReverseMatrix(uint32_t n, double* matrix, double* inv){

	for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            if (i == j)
                inv[i * n + j] = 1.0;
            else
                inv[i * n + j] = 0.0;
        }
    }
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            if (i != j) {
                double ratio = matrix[j * n + i] / matrix[i * n + i];
                for (uint32_t k = 0; k < n; k++) {
                    matrix[j * n + k] -= ratio * matrix[i * n + k];
                }
                for (uint32_t k = 0; k < n; k++) {
                    inv[j * n + k] -= ratio * inv[i * n + k];
                }
            }
        }
    }
    for (uint32_t i = 0; i < n; i++) {
        double a = matrix[i * n + i];
        for (uint32_t j = 0; j < n; j++) {
            matrix[i * n + j] /= a;
        }
        for (uint32_t j = 0; j < n; j++) {
            inv[i * n + j] /= a;
        }
    }
}

double f(uint32_t x, uint32_t y, uint32_t i) {
	//printf("%d,%d %d\n", x, y, i);
	switch(i) {
	case 0:
		return 1;
	case 1:
		return x;
	case 2:
		return y;
	case 3:
		return x * x;
	case 4:
		return y * y;
	case 5:
		return x * y;
	case 6:
		return x * x * x;
	case 7:
		return y * y * y;
	case 8:
		return x * x * y;
	case 9:
		return x * y * y;
	case 10:
		return x * x * x * x;
	case 11:
		return y * y * y * y;
	case 12:
		return x * x * y * y;
	case 13:
		return x * x * x * y;
	case 14:
		return x * y * y * y;
	}
	return 0;
}

double b(uint32_t k, uint32_t i, uint32_t width, uint32_t height) {
	double b = 0;
	for (uint32_t y = 0; y < height; y++) {
		for (uint32_t x = 0; x < width; x++) {
			b += f(x, y, i) * f(x, y, k);
			//printf("[%d,%d]=%lf\n", x, y, b);
		}
	}
	return b;
}

void MakeRegressionMatrix(uint32_t width, uint32_t height, uint32_t n, double* data) {

	double bm[n * n];
	for (uint32_t k = 0; k < n; k++) {
		for (uint32_t i = 0; i < n; i++) {
			bm[k * n + i] = b(k, i, width, height);
		}
	}

	double inv[n * n];
	ReverseMatrix(n, bm, inv);

	double* rm = data;
	for (uint32_t k = 0; k < n; k++) {
		for (uint32_t y = 0; y < height; y++) {
			for (uint32_t x = 0; x < width; x++) {
				double v = 0;
				for (uint32_t i = 0; i < n; i++) {
					v += f(x, y, i) * inv[k * n + i];
				}
				*rm++ = v;
			}
		}
	}
}

/*
y = [0, 1, 2, 3, 4]
s = 1

n = len(y)
sum = n * (n-1) / 2
sum2 = (n - 1) * n * (2 * n - 1) / 6

D = (sum * sum - n * sum2)
Ak = - n / (D * s)
Ab = sum / (D * s)
Bk = sum / D
Bb = - sum2 / D

a = 0
b = 0
for k in range(n):
    a += y[k] * (Ab + Ak * k)
    b += y[k] * (Bb + Bk * k)
print(a, b)
*/

void countLinearMatrix(int32_t n, double& Ak, double& Ab, double& Bk, double& Bb) {
	double sum = n * (n - 1) / 2;
	double sum2 = (n - 1) * n * (2 * n - 1) / 6;
	auto D = (sum * sum - n * sum2);
	Ak = - n / D;
    Ab = sum / D;
    Bk = sum / D;
    Bb = - sum2 / D;
}


/*
void TStatImgSegmentsExtractor::MakeSmoothing() {
	int n = Parameters->RegressionMatrix.Depth;
	double* rm = Parameters->RegressionMatrix.Cell(0, 0);
    double* a = A;
	for (int c = 0; c < Image->Depth; c++) {
		double* rm_ptr = rm;
		for (int k = 0; k < n; k++) {
			double v = 0;
			for (int y = 0; y < Image->Height; y++) {
				for (int x = 0; x < Image->Width; x++) {
					v += Image->Cell(x, y)[c] * *rm_ptr++;
				}
			}
            *a++ = v;
		}
	}
}
*/
