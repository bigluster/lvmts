/*
 * Copyright (C) 2012 Hubert Kario <kario@wsisiz.edu.pl>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <confuse.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <lvm2cmd.h>
#include "config.h"
#include "lvmls.h"

/** Create new program_params with default settings */
struct program_params*
new_program_params()
{
  struct program_params *pp;

  pp = calloc(sizeof(struct program_params),1);
  if (!pp)
    return NULL;

  pp->conf_file_path = strdup("doc/sample.conf");

  pp->cfg = NULL;

  return pp;
}

/** destroy program_params */
void
free_program_params(struct program_params *pp)
{
    if(!pp)
        return;

    if(pp->conf_file_path)
        free(pp->conf_file_path);

    cfg_free(pp->cfg);

    if (pp->lvm2_handle)
        lvm2_exit(pp->lvm2_handle);

    le_to_pe_exit(pp);

    free(pp);
}

float
get_read_multiplier(struct program_params *pp, const char *lv_name)
{
    return cfg_getfloat(cfg_gettsec(pp->cfg, "volume", lv_name),
                         "readMultiplier");
}

float
get_write_multiplier(struct program_params *pp, const char *lv_name)
{
    return cfg_getfloat(cfg_gettsec(pp->cfg, "volume", lv_name),
                         "writeMultiplier");
}

float
get_hit_score(struct program_params *pp, const char *lv_name)
{
    return cfg_getfloat(cfg_gettsec(pp->cfg, "volume", lv_name),
                         "hitScore");
}

float
get_score_scaling_factor(struct program_params *pp, const char *lv_name)
{
    return cfg_getfloat(cfg_gettsec(pp->cfg, "volume", lv_name),
                         "timeExponent");
}

const char *
get_volume_lv(struct program_params *pp, const char *lv_name)
{
    cfg_t *tmp = cfg_gettsec(pp->cfg, "volume", lv_name);
    assert(tmp);

    return cfg_getstr(tmp, "LogicalVolume");
}

const char *
get_volume_vg(struct program_params *pp, const char *lv_name)
{
    cfg_t *tmp = cfg_gettsec(pp->cfg, "volume", lv_name);
    assert(tmp);

    return cfg_getstr(tmp, "VolumeGroup");
}

long int
get_max_space_tier(struct program_params *pp, const char *lv_name,
    int tier)
{
    cfg_t *tmp = cfg_gettsec(pp->cfg, "volume", lv_name);
    assert(tmp);

    cfg_t *pv_cfg;

    for(size_t i=0; i < cfg_size(tmp, "pv"); i++) {
        pv_cfg = cfg_getnsec(tmp, "pv", i);
        if (cfg_getint(pv_cfg, "tier") == tier) {
            return cfg_getint(pv_cfg, "maxUsedSpace");
        }
    }

    return -1;
}

int
lower_tiers_exist(struct program_params *pp, const char *lv_name, int tier)
{
    cfg_t *tmp = cfg_gettsec(pp->cfg, "volume", lv_name);
    assert(tmp);

    int highest_tier;
    cfg_t *pv_cfg = cfg_getnsec(tmp, "pv", 0);
    highest_tier = cfg_getint(pv_cfg, "tier");
    if (highest_tier > tier)
        return 1;

    for (size_t i=1; i < cfg_size(tmp, "pv"); i++) {
        pv_cfg = cfg_getnsec(tmp, "pv", i);
        if (cfg_getint(pv_cfg, "tier") > highest_tier)
            highest_tier = cfg_getint(pv_cfg, "tier");
    }

    if (highest_tier > tier)
        return 1;

    return 0;
}

int
higher_tiers_exist(struct program_params *pp, const char *lv_name, int tier)
{
    cfg_t *tmp = cfg_gettsec(pp->cfg, "volume", lv_name);
    assert(tmp);

    int lowest_tier;
    cfg_t *pv_cfg = cfg_getnsec(tmp, "pv", 0);
    lowest_tier = cfg_getint(pv_cfg, "tier");
    if (lowest_tier < tier)
        return 1;

    for (size_t i=1; i < cfg_size(tmp, "pv"); i++) {
        pv_cfg = cfg_getnsec(tmp, "pv", i);
        if (cfg_getint(pv_cfg, "tier") < lowest_tier)
            lowest_tier = cfg_getint(pv_cfg, "tier");
    }

    if (lowest_tier < tier)
        return 1;

    return 0;
}

const char *
get_tier_device(struct program_params *pp, const char *lv_name, int tier)
{
    cfg_t *tmp = cfg_gettsec(pp->cfg, "volume", lv_name);
    assert(tmp);

    cfg_t *pv_cfg;

    for (size_t i=0; i < cfg_size(tmp, "pv"); i++) {
        pv_cfg = cfg_getnsec(tmp, "pv", i);
        if (cfg_getint(pv_cfg, "tier") == tier)
            return cfg_getstr(pv_cfg, "path");
    }

    return NULL;
}

int
get_device_tier(struct program_params *pp, const char *lv_name, const char *dev)
{
    cfg_t *tmp = cfg_gettsec(pp->cfg, "volume", lv_name);
    assert(tmp);

    cfg_t *pv_cfg;

    for (size_t i=0; i < cfg_size(tmp, "pv"); i++) {
        pv_cfg = cfg_getnsec(tmp, "pv", i);
        if (!strcmp(cfg_getstr(pv_cfg, "path"), dev))
            return cfg_getint(pv_cfg, "tier");
    }
    return -1;
}

float get_tier_pinning_score(struct program_params *pp, const char *lv_name,
    int tier)
{
    cfg_t *vol_cfg = cfg_gettsec(pp->cfg, "volume", lv_name);
    assert(vol_cfg);

    cfg_t *pv_cfg;

    for (size_t i=0; i < cfg_size(vol_cfg, "pv"); i++) {
        pv_cfg = cfg_getnsec(vol_cfg, "pv", i);
        if (cfg_getint(pv_cfg, "tier") == tier)
            return cfg_getfloat(pv_cfg, "pinningScore");
    }

    return 0;
}

// parse time from string, such as "5m", "20s", "3h", "3:10" or "1:15:34"
// as, respectively: 300, 20, 10800, 11400 and 4534
// by default assume minutes
static int
parse_time_value(cfg_t *cfg, cfg_opt_t *opt, const char *value, void *result)
{
    assert(cfg);
    assert(result);
    assert(opt);
    assert(value);

    char *endptr;
    long int *res = (long int *)result;

    long int sum;
    long int partial;

    sum = strtol(value, &endptr, 10);

    // check simple errors while parsing number
    if (endptr == value) {
        cfg_error(cfg, "Invalid value for option %s: value can't be parsed "
            "as a number", opt->name);
        return -1;
    }
    if (sum == LONG_MAX) {
        cfg_error(cfg, "Value too large for option %s.", opt->name);
        return -1;
    }
    if (sum < 0) {
        cfg_error(cfg, "Value can't be negative for option %s.", opt->name);
        return -1;
    }

    // check if a unit has been specified
    switch(*endptr){
      case '\0': // no unit, assume "minutes"
        cfg_error(cfg, "Warning, no unit specified for option %s, assuming "
            "minutes.", opt->name);
        /* fall through */
      case 'm': // m is for "minutes"
      case ':': // so is colon (until the second one, then it's hours)
        sum *= 60;
        break;
      case 'h': // h is for "hours"
        sum *= 3600;
        break;
      case 'd': // d is for "days"
        sum *= 24 * 60 * 60;
        break;
      case 's':
        // do nothing
        break;
      case '\t':
      case ' ':
        cfg_error(cfg, "Whitespace in option %s.", opt->name);
        return -1;
      default:
        cfg_error(cfg, "Unrecognized trailing characters in option %s: %s",
            opt->name, endptr);
        return -1;
    }
    // don't advance past the end of the string
    if (*endptr)
        value = endptr + 1;
    else
        value = endptr;

    partial = strtol(value, &endptr, 10);
    if (endptr == value) {
        // it's OK
    }
    if (partial == LONG_MAX) {
        cfg_error(cfg, "Value too large for option %s.", opt->name);
        return -1;
    }
    if (partial < 0) {
        cfg_error(cfg, "Value can't be negative in option %s.", opt->name);
        return -1;
    }

    switch(*endptr) {
      case '\0': // no unit, assume seconds
        sum += partial;
        break;
      case ':': // we have a "hh:mm:ss" construct...
        sum += partial;
        sum *= 60;
        break;
      case ' ':
      case '\t':
        cfg_error(cfg, "Whitespace in option %s.", opt->name);
        return -1;
      default:
        cfg_error(cfg, "Unrecognized trailing characters in option %s: %s",
            opt->name, endptr);
        return -1;
    }
    // don't advance past the end of the string
    if (*endptr)
        value = endptr + 1;
    else
        value = endptr;

    // parse the last part in "hh:mm:ss" construct
    partial = strtol(value, &endptr, 10);

    if (endptr == value) {
        // OK
    }
    if (partial == LONG_MAX) {
        cfg_error(cfg, "Value too large for option %s.", opt->name);
        return -1;
    }
    if (partial < 0) {
        cfg_error(cfg, "Value can't be negative for option %s.", opt->name);
        return -1;
    }
    if (*endptr != '\0') {
        cfg_error(cfg, "Trailing character(s) in option %s: %s", opt->name,
            endptr);
        return -1;
    }

    sum += partial;

    *res = sum;

    return 0;
}

// parse volume sizes from string, such as "4b", "1k", "4M", "11G"
// as, respectively: 4, 1024, 4194304 and 11811160064
static int
parse_size_value(cfg_t *cfg, cfg_opt_t *opt, const char *value, void *result)
{
    long int res;
    char *endptr;

    res = strtol(value, &endptr, 10);

    // check for simple errors while parsing number
    if (endptr == value) {
        cfg_error(cfg, "Invalid value for option %s: value can't be parsed "
            "as a number.", opt->name);
        return -1;
    }
    if (res == LONG_MAX) {
        cfg_error(cfg, "Value too large for option %s.", opt->name);
        return -1;
    }
    if (res < 0) {
        cfg_error(cfg, "Value can't be negative for option %s.", opt->name);
        return -1;
    }
    if (res == 0) {
        cfg_error(cfg, "Value can't be zero for option %s.", opt->name);
        return -1;
    }

    switch(*endptr) {
        case '\0':
        case 'b':
        case 'B':
          /* do nothing */
          break;
        case 's':
        case 'S':
          res *= 512;
          break;
        case 'k':
        case 'K':
          res *= 1024L;
          break;
        case 'm':
        case 'M':
          res *= 1024L * 1024L;
          break;
        case 'g':
        case 'G':
          res *= 1024L * 1024L * 1024L;
          break;
        case 't':
        case 'T':
          res *= 1024L * 1024L * 1024L * 1024L;
          break;
        default:
          cfg_error(cfg, "Unrecognized trailing characters for option %s: %s",
              opt->name, endptr);
          return -1;
    }
    if (*endptr && *(endptr+1)) {
        cfg_error(cfg, "Trailing characters in option %s: %s", opt->name, endptr);
        return -1;
    }

    *(long int*)result = res;

    return 0;
}

static int
validate_require_nonnegative(cfg_t *cfg, cfg_opt_t *opt)
{
    if (opt->type == CFGT_INT) {
        long int value = cfg_opt_getnint(opt, cfg_opt_size(opt) - 1);
        if (value < 0) {
            cfg_error(cfg, "Value for option %s can't be negative in %s section \"%s\"",
                opt->name, cfg->name, cfg_title(cfg));
            return -1;
        }
    } else if (opt->type == CFGT_FLOAT) {
        double value = cfg_opt_getnfloat(opt, cfg_opt_size(opt) - 1);
        if (value < 0.0) {
            cfg_error(cfg, "Value for option %s can't be negative in %s section \"%s\"",
                opt->name, cfg->name, cfg_title(cfg));
            return -1;
        }
    }

    return 0;
}

static int
validate_require_positive(cfg_t *cfg, cfg_opt_t *opt)
{
    assert(opt->type == CFGT_INT || opt->type == CFGT_FLOAT);

    if (opt->type == CFGT_INT) {
        long int value = cfg_opt_getnint(opt, cfg_opt_size(opt) - 1);
        if (value <= 0) {
            cfg_error(cfg, "Value for option %s must be positive in %s section \"%s\"",
                opt->name, cfg->name, cfg_title(cfg));
            return -1;
        }
    } else if (opt->type == CFGT_FLOAT) {
        double value = cfg_opt_getnfloat(opt, cfg_opt_size(opt) - 1);
        if (value <= 0.0) {
            cfg_error(cfg, "Value for option %s must be positive in %s section \"%s\"",
                opt->name, cfg->name, cfg_title(cfg));
            return -1;
        }
    }
    return 0;
}

/*
 * read configuration file
 */
int
read_config(struct program_params *pp)
{
    static cfg_opt_t pv_opts[] = {
        CFG_INT("tier", 0, CFGF_NONE),
        CFG_FLOAT("pinningScore", 0, CFGF_NONE),
        CFG_STR("path", NULL, CFGF_NONE),
        CFG_INT_CB("maxUsedSpace", -1, CFGF_NONE, parse_size_value),
        CFG_END()
    };

    static cfg_opt_t volume_opts[] = {
        CFG_STR("LogicalVolume", NULL, CFGF_NONE),
        CFG_STR("VolumeGroup",   NULL, CFGF_NONE),
        CFG_FLOAT("timeExponent",  1.0/(2<<14), CFGF_NONE),
        CFG_FLOAT("hitScore",      16, CFGF_NONE),
        CFG_FLOAT("readMultiplier", 1, CFGF_NONE),
        CFG_FLOAT("writeMultiplier", 4, CFGF_NONE),
        CFG_INT_CB("pvmoveWait",     5*60, CFGF_NONE, parse_time_value),
        CFG_INT_CB("checkWait",      15*60, CFGF_NONE, parse_time_value),
        CFG_SEC("pv", pv_opts, CFGF_TITLE | CFGF_MULTI),
        CFG_END()
    };

    cfg_opt_t opts[] = {
        CFG_SEC("volume", volume_opts, CFGF_TITLE | CFGF_MULTI),
        CFG_END()
    };

    cfg_t *cfg;

    cfg = cfg_init(opts, CFGF_NONE);
    assert(cfg);

    // add verification functions
    cfg_set_validate_func(cfg, "volume|timeExponent",
        validate_require_positive);
    cfg_set_validate_func(cfg, "volume|hitScore",
        validate_require_positive);
    cfg_set_validate_func(cfg, "volume|readMultiplier",
        validate_require_nonnegative);
    cfg_set_validate_func(cfg, "volume|writeMultiplier",
        validate_require_nonnegative);
    cfg_set_validate_func(cfg, "volume|pv|pinningScore",
        validate_require_nonnegative);
    cfg_set_validate_func(cfg, "volume|pv|tier",
        validate_require_nonnegative);
    cfg_set_validate_func(cfg, "volume|pv|maxUsedSpace",
        validate_require_nonnegative);
    // TODO cfg_set_validate_func(cfg, "volume", validate_pv); // do they belong to volume, is there enough space

    switch(cfg_parse(cfg, pp->conf_file_path)) {
      case CFG_FILE_ERROR:
        fprintf(stderr, "Configuration file \"%s\" could not be read: %s\n",
            pp->conf_file_path, strerror(errno));
        return 1;
      case CFG_SUCCESS:
        break;
      case CFG_PARSE_ERROR:
        fprintf(stderr, "Configuration file errors, aborting\n");
        return 1;
      default:
        fprintf(stderr, "Internal error");
        assert(0);
    }

    pp->cfg = cfg;

    return 0;
}
