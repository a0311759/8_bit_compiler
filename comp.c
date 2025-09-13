// It is the code of compiler that converts {file}.simxl code to {file}.test_ins code


/* simxl2test.c - .simxl to .test_ins compiler */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <strings.h>

#define MAX_VARS 7
#define MAX_LINE 512
#define MAX_NAME 64

char vars[MAX_VARS][MAX_NAME];
int var_count = 0;

void trim_inplace(char *s) {
    int n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r' || isspace((unsigned char)s[n-1]))) { 
        s[--n] = '\0'; 
    }
    int i = 0;
    while (s[i] && isspace((unsigned char)s[i])) i++;
    if (i) memmove(s, s + i, strlen(s + i) + 1);
}

int is_number(const char *s) {
    if (!s || !*s) return 0;
    int i = 0;
    if (s[0] == '+' || s[0] == '-') i = 1;
    for (; s[i]; ++i) if (!isdigit((unsigned char)s[i])) return 0;
    return 1;
}

int find_var(const char *name) {
    for (int i = 0; i < var_count; ++i) 
        if (strcmp(vars[i], name) == 0) return i;
    return -1;
}

int add_var(const char *name) {
    if (var_count >= MAX_VARS) return -1;
    strncpy(vars[var_count], name, MAX_NAME - 1);
    vars[var_count][MAX_NAME - 1] = '\0';
    return var_count++;
}

void regname(int idx, char *out, size_t outsz) { 
    snprintf(out, outsz, "R%d", idx); 
}

void strip_trailing_semicolon(char *s) {
    trim_inplace(s);
    int n = strlen(s);
    if (n > 0 && s[n-1] == ';') s[n-1] = '\0';
    trim_inplace(s);
}

void strip_degree_marker(char *s) {
    int len = strlen(s);
    if (len >= 3) {
        unsigned char a = (unsigned char)s[len-3];
        unsigned char b = (unsigned char)s[len-2];
        unsigned char c = (unsigned char)s[len-1];
        if (a == 0xC2 && b == 0xB0 && c == '/') {
            s[len-3] = '\0';
            return;
        }
    }
    if (len >= 2) {
        unsigned char b = (unsigned char)s[len-2];
        unsigned char c = (unsigned char)s[len-1];
        if (b == 0xB0 && c == '/') {
            s[len-2] = '\0';
            return;
        }
    }
}

void emit_write_register_or_immediate(FILE *out, int dest_reg, const char *src_token) {
    char dest[8];
    regname(dest_reg, dest, sizeof(dest));
    if (src_token[0] == 'R' && isdigit((unsigned char)src_token[1])) {
        fprintf(out, "WRITE %s, %s\n", dest, src_token);
    } else if (src_token[0] == '"' ) {
        fprintf(out, "WRITE %s, %s\n", dest, src_token);
    } else {
        fprintf(out, "WRITE %s, %s\n", dest, src_token);
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s program.simxl\n", argv[0]);
        return 1;
    }

    const char *inname = argv[1];
    char outname[512];
    char *dot = strrchr((char*)inname, '.');
    if (dot) {
        int base_len = (int)(dot - inname);
        if (base_len > (int)sizeof(outname)-20) base_len = (int)sizeof(outname)-20;
        strncpy(outname, inname, base_len);
        outname[base_len] = '\0';
        strcat(outname, ".test_ins");
    } else {
        snprintf(outname, sizeof(outname), "%s.test_ins", inname);
    }

    FILE *fin = fopen(inname, "r");
    if (!fin) { perror("open input"); return 1; }
    FILE *fout = fopen(outname, "w");
    if (!fout) { perror("open output"); fclose(fin); return 1; }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fin)) {
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';
        trim_inplace(line);
        strip_trailing_semicolon(line);
        if (line[0] == '\0') continue;

        char tmp[MAX_LINE];
        strncpy(tmp, line, sizeof(tmp)-1); 
        tmp[sizeof(tmp)-1] = '\0';
        
        if (strncasecmp(tmp, "var ", 4) == 0) {
            char name[MAX_NAME];
            if (sscanf(line+4, "%63s", name) >= 1) {
                if (find_var(name) < 0) {
                    if (add_var(name) < 0) {
                        fprintf(stderr, "Too many variables (max %d)\n", MAX_VARS);
                        fclose(fin); fclose(fout);
                        return 1;
                    }
                    fprintf(fout, "# var %s -> R%d\n", name, find_var(name));
                }
            }
            continue;
        }

        if (strncasecmp(tmp, "input ", 6) == 0) {
            char name[MAX_NAME];
            if (sscanf(line+6, "%63s", name) >= 1) {
                int idx = find_var(name);
                if (idx < 0) { 
                    idx = add_var(name); 
                    if (idx < 0) { 
                        fprintf(stderr,"Too many vars\n"); 
                        fclose(fin); fclose(fout); return 1; 
                    } 
                }
                char rname[8]; 
                regname(idx, rname, sizeof(rname));
                fprintf(fout, "INPUT %s\n", rname);
            }
            continue;
        }

        if (strncasecmp(tmp, "print ", 6) == 0) {
            char *first_quote = strchr(line, '"');
            if (first_quote) {
                char *last_quote = strrchr(line, '"');
                if (last_quote == first_quote) {
                    fprintf(stderr, "Unterminated string in print\n");
                    fclose(fin); fclose(fout); return 1;
                }
                int clen = (int)(last_quote - first_quote - 1);
                if (clen < 0) clen = 0;
                char content[MAX_LINE];
                if (clen >= (int)sizeof(content)) clen = (int)sizeof(content)-1;
                strncpy(content, first_quote + 1, clen);
                content[clen] = '\0';
                strip_degree_marker(content);
                
                for (int i = 0; content[i] != '\0'; ++i) {
                    char ch = content[i];
                    if (ch == '\\' || ch == '"') {
                        char esc[4] = {'\\', ch, 0, 0};
                        fprintf(fout, "WRITE R7, \"%s\"\n", esc);
                    } else {
                        if ((unsigned char)ch == 0) continue;
                        fprintf(fout, "WRITE R7, \"%c\"\n", ch);
                    }
                    fprintf(fout, "PRINT R7\n");
                }
            } else {
                char name[MAX_NAME];
                if (sscanf(line+6, "%63s", name) >= 1) {
                    int idx = find_var(name);
                    if (idx < 0) { 
                        idx = add_var(name); 
                        if (idx < 0) { 
                            fprintf(stderr,"Too many vars\n"); 
                            fclose(fin); fclose(fout); return 1; 
                        } 
                    }
                    char rname[8]; 
                    regname(idx, rname, sizeof(rname));
                    fprintf(fout, "PRINT %s\n", rname);
                }
            }
            continue;
        }

        if (strncasecmp(tmp, "if ", 3) == 0) {
            char left[64], op[4], right[64];
            if (sscanf(line+3, "%63s %3s %63s", left, op, right) >= 2) {
                int li = find_var(left);
                if (li < 0) { 
                    li = add_var(left); 
                    if (li < 0) { 
                        fprintf(stderr,"Too many vars\n"); 
                        fclose(fin); fclose(fout); return 1; 
                    } 
                }
                char left_reg[8]; 
                regname(li, left_reg, sizeof(left_reg));
                
                char right_tok[128];
                if (is_number(right)) {
                    snprintf(right_tok, sizeof(right_tok), "%s", right);
                } else {
                    int ri = find_var(right);
                    if (ri < 0) { 
                        ri = add_var(right); 
                        if (ri < 0) { 
                            fprintf(stderr,"Too many vars\n"); 
                            fclose(fin); fclose(fout); return 1; 
                        } 
                    }
                    char rreg[8]; 
                    regname(ri, rreg, sizeof(rreg));
                    snprintf(right_tok, sizeof(right_tok), "%s", rreg);
                }
                fprintf(fout, "IF %s %s %s\n", left_reg, op, right_tok);
            } else {
                fprintf(stderr, "Invalid if syntax: %s\n", line);
                fclose(fin); fclose(fout); return 1;
            }
            continue;
        }

        if (strncasecmp(tmp, "else", 4) == 0) {
            fprintf(fout, "ELSE\n");
            continue;
        }

        if (strchr(line, '=')) {
            char left[64], expr[MAX_LINE];
            if (sscanf(line, " %63[^=] = %511[^\n]", left, expr) >= 2) {
                trim_inplace(left);
                trim_inplace(expr);
                strip_trailing_semicolon(left);
                strip_trailing_semicolon(expr);
                int dest = find_var(left);
                if (dest < 0) { 
                    dest = add_var(left); 
                    if (dest < 0) { 
                        fprintf(stderr,"Too many vars\n"); 
                        fclose(fin); fclose(fout); return 1; 
                    } 
                }

                char opch = 0;
                int oppos = -1;
                for (int i = 0; expr[i] != '\0'; ++i) {
                    if (expr[i] == '+' || expr[i] == '-' || expr[i] == '*' || expr[i] == '/') {
                        opch = expr[i];
                        oppos = i;
                        break;
                    }
                }
                
                if (opch == 0) {
                    if (is_number(expr)) {
                        char destreg[8]; 
                        regname(dest, destreg, sizeof(destreg));
                        fprintf(fout, "WRITE %s, %s\n", destreg, expr);
                    } else {
                        char token[64]; 
                        strncpy(token, expr, sizeof(token)-1); 
                        token[sizeof(token)-1]=0; 
                        trim_inplace(token);
                        int src = find_var(token);
                        if (src < 0) { 
                            src = add_var(token); 
                            if (src < 0) { 
                                fprintf(stderr,"Too many vars\n"); 
                                fclose(fin); fclose(fout); return 1; 
                            } 
                        }
                        char sreg[8], dreg[8]; 
                        regname(src, sreg, sizeof(sreg)); 
                        regname(dest, dreg, sizeof(dreg));
                        fprintf(fout, "WRITE %s, %s\n", dreg, sreg);
                    }
                } else {
                    char leftop[128], rightop[128];
                    int i;
                    for (i = 0; i < oppos; ++i) leftop[i] = expr[i];
                    leftop[i] = '\0';
                    strncpy(rightop, expr + oppos + 1, sizeof(rightop)-1); 
                    rightop[sizeof(rightop)-1] = 0;
                    trim_inplace(leftop); 
                    trim_inplace(rightop);
                    
                    int left_is_num = is_number(leftop);
                    int right_is_num = is_number(rightop);
                    
                    if (left_is_num && right_is_num) {
                        long a = atol(leftop);
                        long b = atol(rightop);
                        long res = 0;
                        switch (opch) {
                            case '+': res = a + b; break;
                            case '-': res = a - b; break;
                            case '*': res = a * b; break;
                            case '/': res = (b == 0) ? 0 : a / b; break;
                        }
                        char dreg[8]; 
                        regname(dest, dreg, sizeof(dreg));
                        fprintf(fout, "WRITE %s, %ld\n", dreg, res);
                    } else {
                        char areg[8], breg[8];
                        if (!left_is_num) {
                            int ai = find_var(leftop);
                            if (ai < 0) { 
                                ai = add_var(leftop); 
                                if (ai < 0) { 
                                    fprintf(stderr,"Too many vars\n"); 
                                    fclose(fin); fclose(fout); return 1; 
                                } 
                            }
                            regname(ai, areg, sizeof(areg));
                        } else {
                            regname(7, areg, sizeof(areg));
                            fprintf(fout, "WRITE %s, %s\n", areg, leftop);
                        }
                        
                        if (!right_is_num) {
                            int bi = find_var(rightop);
                            if (bi < 0) { 
                                bi = add_var(rightop); 
                                if (bi < 0) { 
                                    fprintf(stderr,"Too many vars\n"); 
                                    fclose(fin); fclose(fout); return 1; 
                                } 
                            }
                            regname(bi, breg, sizeof(breg));
                        } else {
                            regname(7, breg, sizeof(breg));
                            fprintf(fout, "WRITE %s, %s\n", breg, rightop);
                        }

                        const char *mop = NULL;
                        if (opch == '+') mop = "ADD";
                        else if (opch == '-') mop = "SUB";
                        else if (opch == '*') mop = "MUL";
                        else if (opch == '/') mop = "DIV";
                        if (!mop) { 
                            fprintf(stderr, "Unknown operator %c\n", opch); 
                            fclose(fin); fclose(fout); return 1; 
                        }

                        char dreg[8]; 
                        regname(dest, dreg, sizeof(dreg));
                        fprintf(fout, "%s %s, %s, %s\n", mop, dreg, areg, breg);
                    }
                }
                continue;
            } else {
                fprintf(stderr, "Bad assignment line: %s\n", line);
                fclose(fin); fclose(fout);
                return 1;
            }
        }

        fprintf(stderr, "Unknown or unsupported line: %s\n", line);
        fclose(fin); fclose(fout);
        return 1;
    }

    fclose(fin);
    fclose(fout);
    printf("Compiled %s -> %s\n", inname, outname);
    if (var_count > 0) {
        printf("Variables mapping:\n");
        for (int i = 0; i < var_count; ++i) {
            printf("  %s -> R%d\n", vars[i], i);
        }
    }
    return 0;
}
