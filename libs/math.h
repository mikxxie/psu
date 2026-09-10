#ifndef MATH_H
#define MATH_H

int contains_math_expression(const char *str);
double evaluate_expression(const char *expr, int *error);
void load_math_library();

#endif
