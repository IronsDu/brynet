/*
Copyright (C) <2015>  <sniperHW@163.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define REDIS_REPLY_STRING  1
#define REDIS_REPLY_ARRAY   2
#define REDIS_REPLY_INTEGER 3
#define REDIS_REPLY_NIL     4
#define REDIS_REPLY_STATUS  5
#define REDIS_REPLY_ERROR   6


#define REDIS_OK     0
#define REDIS_RETRY -1
#define REDIS_ERR   -2    

struct redisReply {
    int32_t      type;    /* REDIS_REPLY_* */
    int64_t      integer; /* The integer when type is REDIS_REPLY_INTEGER */
    int32_t      len;     /* Length of string */
    char        *str;     /* Used for both REDIS_REPLY_ERROR and REDIS_REPLY_STRING */
    size_t       elements;/* number of elements, for REDIS_REPLY_ARRAY */
    redisReply **element; /* elements vector for REDIS_REPLY_ARRAY */
};



#define SIZE_TMP_BUFF 512 //size must enough for status/error string

struct parse_tree {
    redisReply         *reply;
    parse_tree        **childs;
    size_t              want;
    size_t 				pos;
    char                type;
    char                break_;
    char   				tmp_buff[SIZE_TMP_BUFF];
};

#define IS_NUM(CC) (CC >= '0' && CC <= '9')

#define PARSE_SET(FIELD)								    \
		reply->FIELD = (reply->FIELD*10)+(c - '0');			\

#define PARSE_NUM(FIELD)								  ({\
int ret = 0;												\
do{         												\
	if(IS_NUM(c)) {											\
		reply->FIELD = (reply->FIELD*10)+(c - '0');			\
		ret = 1;											\
        	}														\
}while(0);ret;                                             })

static int32_t parse_string(parse_tree *current, char **str, char *end) {
    char c, termi;
    redisReply *reply = current->reply;
    if (!reply->str) reply->str = current->tmp_buff;
    if (current->want) {
        //带了长度的string
        for (c = **str; *str != end && current->want; ++(*str), c = **str, --current->want)
            if (current->want > 2) reply->str[current->pos++] = c;//结尾的\r\n不需要
        if (current->want) return REDIS_RETRY;
    }
    else {
        for (;;) {
            termi = current->break_;
            for (c = **str; *str != end && c != termi; ++(*str), c = **str)
                reply->str[current->pos++] = c;
            if (*str == end) return REDIS_RETRY;
            ++(*str);
            if (termi == '\n') break;
            else current->break_ = '\n';
        }
        reply->len = current->pos;
    }
    assert(reply->len == current->pos);
    reply->str[current->pos] = 0;
    return REDIS_OK;
}

static int32_t parse_integer(parse_tree *current, char **str, char *end) {
    char c, termi;
    redisReply *reply = current->reply;
    for (;;) {
        termi = current->break_;
        for (c = **str; *str != end && c != termi; ++(*str), c = **str)
            if (c == '-') current->want = -1;
            else if (IS_NUM(c))
            {
                PARSE_SET(integer);
            }
            else
                return REDIS_ERR;
            if (*str == end) return REDIS_RETRY;
            ++(*str);
            if (termi == '\n') break;
            else current->break_ = '\n';
    }
    reply->integer *= current->want;
    return REDIS_OK;
}

static int32_t parse_breply(parse_tree *current, char **str, char *end) {
    char c, termi;
    redisReply *reply = current->reply;
    if (!current->want) {
        for (;;) {
            termi = current->break_;
            for (c = **str; *str != end && c != termi; ++(*str), c = **str)
                if (c == '-') reply->type = REDIS_REPLY_NIL;
                else if (IS_NUM(c))
                {
                    PARSE_SET(len);
                }
                else
                {
                    return REDIS_ERR;
                }

                if (*str == end) return REDIS_RETRY;
                ++(*str);
                if (termi == '\n') {
                    current->break_ = '\r';
                    break;
                }
                else current->break_ = '\n';
        };
        if (reply->type == REDIS_REPLY_NIL) return REDIS_OK;
        current->want = reply->len + 2;//加上\r\n
    }

    if (!reply->str && reply->len + 1 > SIZE_TMP_BUFF)
        reply->str = (char*)calloc(1, reply->len + 1);
    return parse_string(current, str, end);
}

static parse_tree *parse_tree_new() {
    parse_tree *tree = (parse_tree *)calloc(1, sizeof(*tree));
    tree->reply = (redisReply *)calloc(1, sizeof(*tree->reply));
    tree->break_ = '\r';
    return tree;
}

static void parse_tree_del(parse_tree *tree) {
    size_t i;
    if (tree->childs) {
        for (i = 0; i < tree->want; ++i)
            parse_tree_del(tree->childs[i]);
        free(tree->childs);
        free(tree->reply->element);
    }
    if (tree->reply->str != tree->tmp_buff) free(tree->reply->str);
    free(tree->reply);
    free(tree);
}

static int32_t parse(parse_tree *current, char **str, char *end);

static int32_t parse_mbreply(parse_tree *current, char **str, char *end) {
    size_t  i;
    int32_t ret;
    char    c, termi;
    redisReply *reply = current->reply;
    if (!current->want) {
        for (;;) {
            termi = current->break_;
            for (c = **str; *str != end && c != termi; ++(*str), c = **str)
                if (c == '-') reply->type = REDIS_REPLY_NIL;
                else if (IS_NUM(c))
                {
                    PARSE_SET(elements);
                }
                else
                {
                    return REDIS_ERR;
                }

                if (*str == end) return REDIS_RETRY;
                ++(*str);
                if (termi == '\n'){
                    current->break_ = '\r';
                    break;
                }
                else current->break_ = '\n';
        };
        current->want = reply->elements;
    }

    if (current->want > 0 && !current->childs) {
        current->childs = (parse_tree**)calloc(current->want, sizeof(*current->childs));
        reply->element = (redisReply**)calloc(current->want, sizeof(*reply->element));
        for (i = 0; i < current->want; ++i){
            current->childs[i] = parse_tree_new();
            reply->element[i] = current->childs[i]->reply;
        }
    }

    for (; current->pos < current->want; ++current->pos) {
        if ((*str) == end) return REDIS_RETRY;
        if (REDIS_OK != (ret = parse(current->childs[current->pos], str, end)))
            return ret;
    }
    return REDIS_OK;
}

#define IS_OP_CODE(CC)\
 (CC == '+'  || CC == '-'  || CC == ':'  || CC == '$'  || CC == '*')

static int32_t parse(parse_tree *current, char **str, char *end) {
    int32_t ret = REDIS_RETRY;
    redisReply *reply = current->reply;
    if (!current->type) {
        char c = *(*str)++;
        if (IS_OP_CODE(c)) current->type = c;
        else return REDIS_ERR;
    }
    switch (current->type) {
        case '+':{
            if (!reply->type) reply->type = REDIS_REPLY_STATUS;
            ret = parse_string(current, str, end);
            break;
        }
        case '-':{
            if (!reply->type) reply->type = REDIS_REPLY_ERROR;
            ret = parse_string(current, str, end);
            break;
        }
        case ':':{
            if (!reply->type) reply->type = REDIS_REPLY_INTEGER;
            current->want = 1;
            ret = parse_integer(current, str, end);
            break;
        }
        case '$':{
            if (!reply->type) reply->type = REDIS_REPLY_STRING;
            ret = parse_breply(current, str, end);
            break;
        }
        case '*':{
            if (!reply->type) reply->type = REDIS_REPLY_ARRAY;
            ret = parse_mbreply(current, str, end);
            break;
        }
        default:
            return REDIS_ERR;
    }
    return ret;
}


static inline size_t digitcount(uint32_t num) {
    if (num < 10) return 1;
    else if (num < 100) return 2;
    else if (num < 1000) return 3;
    else if (num < 10000) return 4;
    else if (num < 100000) return 5;
    else if (num < 1000000) return 6;
    else if (num < 10000000) return 7;
    else if (num < 100000000) return 8;
    else if (num < 1000000000) return 9;
    else return 10;
}

static inline void u2s(uint32_t num, char **ptr) {
    char *tmp = *ptr + digitcount(num);
    do {
        *--tmp = '0' + (char)(num % 10);
        (*ptr)++;
    } while (num /= 10);
}


#endif
