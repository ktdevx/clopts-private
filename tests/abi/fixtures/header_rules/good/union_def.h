/* Allowed: a union definition without typedef, inside extern "C". */
#ifndef GOOD_UNION_DEF_H
#define GOOD_UNION_DEF_H

#ifdef __cplusplus
extern "C" {
#endif

union good_value { int i; double d; };

#ifdef __cplusplus
}
#endif

#endif /* GOOD_UNION_DEF_H */
