#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

void count_ifs(cJSON *node, int *if_count) {
    if (!node) return;

    if (cJSON_IsObject(node)) {
        cJSON *ntype = cJSON_GetObjectItem(node, "_nodetype");
        if (ntype && cJSON_IsString(ntype) && strcmp(ntype->valuestring, "If") == 0) {
            (*if_count)++;
        }

        cJSON *child = node->child;
        while (child) {
            count_ifs(child, if_count);
            child = child->next;
        }
    } else if (cJSON_IsArray(node)) {
        int len = cJSON_GetArraySize(node);
        for (int i = 0; i < len; ++i) {
            count_ifs(cJSON_GetArrayItem(node, i), if_count);
        }
    }
}

void analyze_function(cJSON *node, int *func_count, int *if_total_count) {
    if (!node) return;

    cJSON *nodetype = cJSON_GetObjectItem(node, "_nodetype");
    if (!nodetype || !cJSON_IsString(nodetype) || strcmp(nodetype->valuestring, "FuncDef") != 0)
        return;

    (*func_count)++;

    cJSON *decl = cJSON_GetObjectItem(node, "decl");
    cJSON *funcdecl = decl ? cJSON_GetObjectItem(decl, "type") : NULL;

    // 함수 이름 추출
    const char *func_name = "<anonymous>";
    cJSON *funcdecl_type = funcdecl ? cJSON_GetObjectItem(funcdecl, "type") : NULL;
    if (funcdecl_type && cJSON_IsObject(funcdecl_type)) {
        cJSON *declname = cJSON_GetObjectItem(funcdecl_type, "declname");
        if (declname && cJSON_IsString(declname)) {
            func_name = declname->valuestring;
        }
    }
    printf("Function: %s\n", func_name);

    // 리턴 타입 추출
    const char *ret_type = "<unknown>";
    if (funcdecl_type) {
        cJSON *rettype = cJSON_GetObjectItem(funcdecl_type, "type"); // IdentifierType
        if (rettype) {
            cJSON *names = cJSON_GetObjectItem(rettype, "names");
            if (names && cJSON_IsArray(names) && cJSON_GetArraySize(names) > 0) {
                cJSON *n0 = cJSON_GetArrayItem(names, 0);
                if (n0 && cJSON_IsString(n0)) {
                    ret_type = n0->valuestring;
                }
            }
        }
    }
    printf("  Return Type: %s\n", ret_type);

    // 파라미터 추출
    printf("  Parameters:\n");
    cJSON *args = funcdecl ? cJSON_GetObjectItem(funcdecl, "args") : NULL;
    if (args && cJSON_IsObject(args)) {
        cJSON *params = cJSON_GetObjectItem(args, "params");
        if (params && cJSON_IsArray(params)) {
            int plen = cJSON_GetArraySize(params);
            for (int i = 0; i < plen; ++i) {
                cJSON *param = cJSON_GetArrayItem(params, i);
                const char *pname = "<unknown>";
                const char *ptype_str = "<unknown>";

                // 이름
                cJSON *p_name = cJSON_GetObjectItem(param, "name");
                if (p_name && cJSON_IsString(p_name)) pname = p_name->valuestring;

                // 타입
                cJSON *p_type = cJSON_GetObjectItem(param, "type");
                while (p_type && cJSON_IsObject(p_type)) {
                    cJSON *inner = cJSON_GetObjectItem(p_type, "type");
                    if (!inner) break;

                    cJSON *names = cJSON_GetObjectItem(inner, "names");
                    if (names && cJSON_IsArray(names) && cJSON_GetArraySize(names) > 0) {
                        cJSON *n0 = cJSON_GetArrayItem(names, 0);
                        if (n0 && cJSON_IsString(n0)) {
                            ptype_str = n0->valuestring;
                            break;
                        }
                    }
                    p_type = inner;
                }

                printf("    - %s %s\n", ptype_str, pname);
            }
        } else {
            printf("    (no parameters)\n");
        }
    } else {
        printf("    (void or no parameter list)\n");
    }

    // if문 개수 세기
    cJSON *body = cJSON_GetObjectItem(node, "body");
    int local_if_count = 0;
    if (body) {
        count_ifs(body, &local_if_count);
        *if_total_count += local_if_count;
    }
    printf("  If Count: %d\n\n", local_if_count);
}

int main() {
    FILE *f = fopen("ast.json", "rb");
    if (!f) {
        perror("Cannot open file");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *data = (char*)malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(data);
    if (!root) {
        fprintf(stderr, "JSON parse error\n");
        free(data);
        return 1;
    }

    int func_count = 0;
    int if_total = 0;

    cJSON *ext = cJSON_GetObjectItem(root, "ext");
    if (ext && cJSON_IsArray(ext)) {
        int n = cJSON_GetArraySize(ext);
        for (int i = 0; i < n; ++i) {
            analyze_function(cJSON_GetArrayItem(ext, i), &func_count, &if_total);
        }
    }

    printf("=== Summary ===\n");
    printf("Total Functions: %d\n", func_count);
    printf("Total If Conditions: %d\n", if_total);

    cJSON_Delete(root);
    free(data);
    return 0;
}
