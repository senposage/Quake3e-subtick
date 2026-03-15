/* ANSI-C code produced by gperf version 3.0.1 */
/* Command-line: gperf.exe -L ANSI-C -d code/common/c_arieshl.gperf  */
/* Computed positions: -k'3,6' */


#line 2 "code/common/c_arieshl.gperf"

#ifndef Q3_VM
#include <string.h>
#else
#include "../game/q_shared.h"
#endif
#include "c_arieshl.h"

char *c_ariesHitLocationNames[HL_MAX] = {
    "Unknown",
    "Head",
    "Helmet",
    "Torso",
    "Vest",
    "Left Arm",
    "Right Arm",
    "Groin",
    "Butt",
    "Left Upper Leg",
    "Right Upper Leg",
    "Left Lower Leg",
    "Right Lower Leg",
    "Left Foot",
    "Right Foot"
};

#line 29 "code/common/c_arieshl.gperf"
struct ariesHitLocationMap_s;

#define TOTAL_KEYWORDS 14
#define MIN_WORD_LENGTH 6
#define MAX_WORD_LENGTH 8
#define MIN_HASH_VALUE 6
#define MAX_HASH_VALUE 31
/* maximum key range = 26, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
hash (register const char *str, register unsigned int len)
{
  static unsigned char asso_values[] =
    {
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 10, 15, 32,
       4, 32, 32,  0,  5,  0, 32, 32,  5,  5,
      32,  5, 32, 32,  0,  5, 10, 32, 10, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32
    };
  return len + asso_values[(unsigned char)str[5]] + asso_values[(unsigned char)str[2]];
}

#ifdef __GNUC__
__inline
#endif
struct ariesHitLocationMap_s *
in_word_set (register const char *str, register unsigned int len)
{
  static struct ariesHitLocationMap_s wordlist[] =
    {
      {""}, {""}, {""}, {""}, {""}, {""},
#line 42 "code/common/c_arieshl.gperf"
      {"l_rleg",HL_LEGLR} /* hash value = 6, index = 6 */,
#line 37 "code/common/c_arieshl.gperf"
      {"l_groin",HL_GROIN} /* hash value = 7, index = 7 */,
#line 40 "code/common/c_arieshl.gperf"
      {"l_rthigh",HL_LEGUR} /* hash value = 8, index = 8 */,
      {""}, {""},
#line 41 "code/common/c_arieshl.gperf"
      {"l_lleg",HL_LEGLL} /* hash value = 11, index = 11 */,
#line 44 "code/common/c_arieshl.gperf"
      {"l_rfoot",HL_FOOTR} /* hash value = 12, index = 12 */,
#line 39 "code/common/c_arieshl.gperf"
      {"l_lthigh",HL_LEGUL} /* hash value = 13, index = 13 */,
      {""},
#line 31 "code/common/c_arieshl.gperf"
      {"h_head",HL_HEAD} /* hash value = 15, index = 15 */,
#line 36 "code/common/c_arieshl.gperf"
      {"u_armr",HL_ARMR} /* hash value = 16, index = 16 */,
#line 43 "code/common/c_arieshl.gperf"
      {"l_lfoot",HL_FOOTL} /* hash value = 17, index = 17 */,
#line 32 "code/common/c_arieshl.gperf"
      {"g_helmet",HL_HELMET} /* hash value = 18, index = 18 */,
      {""}, {""},
#line 35 "code/common/c_arieshl.gperf"
      {"u_arml",HL_ARML} /* hash value = 21, index = 21 */,
#line 33 "code/common/c_arieshl.gperf"
      {"u_torso",HL_TORSO} /* hash value = 22, index = 22 */,
      {""}, {""}, {""},
#line 34 "code/common/c_arieshl.gperf"
      {"g_vest",HL_VEST} /* hash value = 26, index = 26 */,
      {""}, {""}, {""}, {""},
#line 38 "code/common/c_arieshl.gperf"
      {"l_butt",HL_BUTT} /* hash value = 31, index = 31 */
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register const char *s = wordlist[key].name;

          if (*str == *s && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}
