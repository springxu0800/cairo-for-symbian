
#define COMPONENT_SIZE
#define MASK
#define ONE_HALF

#define A_SHIFT
#define R_SHIFT
#define G_SHIFT
#define A_MASK
#define R_MASK
#define G_MASK

#define RB_MASK
#define AG_MASK
#define RB_ONE_HALF
#define RB_MASK_PLUS_ONE

#define Alpha(x) ((x) >> A_SHIFT)

/*
 * Helper macros.
 */

#define IntMult(a,b,t) ( (t) = (a) * (b) + ONE_HALF, ( ( ( (t)>>G_SHIFT ) + (t) )>>G_SHIFT ) )
#define IntDiv(a,b)    (((comp2_t) (a) * MASK) / (b))

#define GetComp(v,i)   ((comp2_t) (comp1_t) ((v) >> i))

#define Add(x,y,i,t)   ((t) = GetComp(x,i) + GetComp(y,i),              \
                        (comp4_t) ((comp1_t) ((t) | (0 - ((t) >> G_SHIFT)))) << (i))

#define FbGen(x,y,i,ax,ay,t,u,v) ((t) = (IntMult(GetComp(y,i),ay,(u)) + \
					 IntMult(GetComp(x,i),ax,(v))), \
				  (comp4_t) ((comp1_t) ((t) |		\
							 (0 - ((t) >> G_SHIFT)))) << (i))

/*
  The methods below use some tricks to be able to do two color
  components at the same time.
*/

/*
  x_c = (x_c * a) / 255
*/
#define FbByteMul(x, a) do {                                            \
        comp4_t t = ((x & RB_MASK) * a) + RB_ONE_HALF;                  \
        t = (t + ((t >> COMPONENT_SIZE) & RB_MASK)) >> COMPONENT_SIZE;  \
        t &= RB_MASK;                                                   \
                                                                        \
        x = (((x >> COMPONENT_SIZE) & RB_MASK) * a) + RB_ONE_HALF;      \
        x = (x + ((x >> COMPONENT_SIZE) & RB_MASK));                    \
        x &= RB_MASK << COMPONENT_SIZE;                                 \
        x += t;                                                         \
    } while (0)

/*
  x_c = (x_c * a) / 255 + y
*/
#define FbByteMulAdd(x, a, y) do {                                      \
        /* multiply and divide: trunc((i + 128)*257/65536) */           \
        comp4_t t = ((x & RB_MASK) * a) + RB_ONE_HALF;                  \
        t = (t + ((t >> COMPONENT_SIZE) & RB_MASK)) >> COMPONENT_SIZE;  \
        t &= RB_MASK;                                                   \
                                                                        \
        /* add */                                                       \
        t += y & RB_MASK;                                               \
                                                                        \
        /* saturate */                                                  \
        t |= RB_MASK_PLUS_ONE - ((t >> COMPONENT_SIZE) & RB_MASK);      \
        t &= RB_MASK;                                                   \
                                                                        \
        /* multiply and divide */                                       \
        x = (((x >> COMPONENT_SIZE) & RB_MASK) * a) + RB_ONE_HALF;      \
        x = (x + ((x >> COMPONENT_SIZE) & RB_MASK)) >> COMPONENT_SIZE;  \
        x &= RB_MASK;                                                   \
                                                                        \
        /* add */                                                       \
        x += (y >> COMPONENT_SIZE) & RB_MASK;                           \
                                                                        \
        /* saturate */                                                  \
        x |= RB_MASK_PLUS_ONE - ((x >> COMPONENT_SIZE) & RB_MASK);      \
        x &= RB_MASK;                                                   \
                                                                        \
        /* recombine */                                                 \
        x <<= COMPONENT_SIZE;                                           \
        x += t;                                                         \
    } while (0)

/*
  x_c = (x_c * a + y_c * b) / 255
*/
#define FbByteAddMul(x, a, y, b) do {                                   \
        comp4_t t;                                                      \
        comp4_t r = (x >> A_SHIFT) * a + (y >> A_SHIFT) * b + ONE_HALF; \
        r += (r >> G_SHIFT);                                            \
        r >>= G_SHIFT;                                                  \
                                                                        \
        t = (x & G_MASK) * a + (y & G_MASK) * b;                        \
        t += (t >> G_SHIFT) + (ONE_HALF << G_SHIFT);                    \
        t >>= R_SHIFT;                                                  \
                                                                        \
        t |= r << R_SHIFT;                                              \
        t |= RB_MASK_PLUS_ONE - ((t >> G_SHIFT) & RB_MASK);             \
        t &= RB_MASK;                                                   \
        t <<= G_SHIFT;                                                  \
                                                                        \
        r = ((x >> R_SHIFT) & MASK) * a +                               \
            ((y >> R_SHIFT) & MASK) * b + ONE_HALF;                     \
        r += (r >> G_SHIFT);                                            \
        r >>= G_SHIFT;                                                  \
                                                                        \
        x = (x & MASK) * a + (y & MASK) * b + ONE_HALF;                 \
        x += (x >> G_SHIFT);                                            \
        x >>= G_SHIFT;                                                  \
        x |= r << R_SHIFT;                                              \
        x |= RB_MASK_PLUS_ONE - ((x >> G_SHIFT) & RB_MASK);             \
        x &= RB_MASK;                                                   \
        x |= t;                                                         \
    } while (0)

/*
  x_c = (x_c * a_c) / 255
*/
#define FbByteMulC(x, a) do {                                           \
        comp4_t t;                                                      \
        comp4_t r = (x & MASK) * (a & MASK);                            \
        r |= (x & R_MASK) * ((a >> R_SHIFT) & MASK);                    \
        r += RB_ONE_HALF;                                               \
        r = (r + ((r >> G_SHIFT) & RB_MASK)) >> G_SHIFT;                \
        r &= RB_MASK;                                                   \
                                                                        \
        x >>= G_SHIFT;                                                  \
        t = (x & MASK) * ((a >> G_SHIFT) & MASK);                       \
        t |= (x & R_MASK) * (a >> A_SHIFT);                             \
        t += RB_ONE_HALF;                                               \
        t = t + ((t >> G_SHIFT) & RB_MASK);                             \
        x = r | (t & AG_MASK);                                          \
    } while (0)

/*
  x_c = (x_c * a) / 255 + y
*/
#define FbByteMulAddC(x, a, y) do {                                     \
        comp4_t t;                                                      \
        comp4_t r = (x & MASK) * (a & MASK);                            \
        r |= (x & R_MASK) * ((a >> R_SHIFT) & MASK);                    \
        r += RB_ONE_HALF;                                               \
        r = (r + ((r >> G_SHIFT) & RB_MASK)) >> G_SHIFT;                \
        r &= RB_MASK;                                                   \
        r += y & RB_MASK;                                               \
        r |= RB_MASK_PLUS_ONE - ((r >> G_SHIFT) & RB_MASK);             \
        r &= RB_MASK;                                                   \
                                                                        \
        x >>= G_SHIFT;                                                  \
        t = (x & MASK) * ((a >> G_SHIFT) & MASK);                       \
        t |= (x & R_MASK) * (a >> A_SHIFT);                             \
        t += RB_ONE_HALF;                                               \
        t = (t + ((t >> G_SHIFT) & RB_MASK)) >> G_SHIFT;                \
        t &= RB_MASK;                                                   \
        t += (y >> G_SHIFT) & RB_MASK;                                  \
        t |= RB_MASK_PLUS_ONE - ((t >> G_SHIFT) & RB_MASK);             \
        t &= RB_MASK;                                                   \
        x = r | (t << G_SHIFT);                                         \
    } while (0)

/*
  x_c = (x_c * a_c + y_c * b) / 255
*/
#define FbByteAddMulC(x, a, y, b) do {                                  \
        comp4_t t;                                                      \
        comp4_t r = (x >> A_SHIFT) * (a >> A_SHIFT) +                   \
                     (y >> A_SHIFT) * b;                                \
        r += (r >> G_SHIFT) + ONE_HALF;                                 \
        r >>= G_SHIFT;                                                  \
                                                                        \
        t = (x & G_MASK) * ((a >> G_SHIFT) & MASK) + (y & G_MASK) * b;  \
        t += (t >> G_SHIFT) + (ONE_HALF << G_SHIFT);                    \
        t >>= R_SHIFT;                                                  \
                                                                        \
        t |= r << R_SHIFT;                                              \
        t |= RB_MASK_PLUS_ONE - ((t >> G_SHIFT) & RB_MASK);             \
        t &= RB_MASK;                                                   \
        t <<= G_SHIFT;                                                  \
                                                                        \
        r = ((x >> R_SHIFT) & MASK) * ((a >> R_SHIFT) & MASK) +         \
            ((y >> R_SHIFT) & MASK) * b + ONE_HALF;                     \
        r += (r >> G_SHIFT);                                            \
        r >>= G_SHIFT;                                                  \
                                                                        \
        x = (x & MASK) * (a & MASK) + (y & MASK) * b + ONE_HALF;        \
        x += (x >> G_SHIFT);                                            \
        x >>= G_SHIFT;                                                  \
        x |= r << R_SHIFT;                                              \
        x |= RB_MASK_PLUS_ONE - ((x >> G_SHIFT) & RB_MASK);             \
        x &= RB_MASK;                                                   \
        x |= t;                                                         \
    } while (0)

/*
  x_c = min(x_c + y_c, 255)
*/
#define FbByteAdd(x, y) do {                                            \
        comp4_t t;                                                      \
        comp4_t r = (x & RB_MASK) + (y & RB_MASK);                      \
        r |= RB_MASK_PLUS_ONE - ((r >> G_SHIFT) & RB_MASK);             \
        r &= RB_MASK;                                                   \
                                                                        \
        t = ((x >> G_SHIFT) & RB_MASK) + ((y >> G_SHIFT) & RB_MASK);    \
        t |= RB_MASK_PLUS_ONE - ((t >> G_SHIFT) & RB_MASK);             \
        r |= (t & RB_MASK) << G_SHIFT;                                  \
        x = r;                                                          \
    } while (0)

