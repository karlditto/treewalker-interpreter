#include <complex.h>
#include <ctype.h>
#include <math.h>
#include <readline/chardefs.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// bumper allocator
#define REIGON_INIT_SIZE 1024 * 1024
typedef struct {
  char *data;
  size_t size;
  size_t used;
} Region;
void region_init(Region *region) {
  region->data = calloc(1, REIGON_INIT_SIZE);
  if (region->data == NULL)
    return;
  region->size = REIGON_INIT_SIZE;
  region->used = 0;
}
void *region_alloc(size_t size, Region *region) {
  if (region == NULL)
    return NULL;
  if (region->data == NULL)
    return NULL;
  if (region->used + size > region->size)
    return NULL;
  // default alignment 8;
  void *ptr = region->data + region->used;
  size_t pad = (unsigned long)ptr % 8 == 0 ? 0 : 8 - (unsigned long)ptr % 8;
  region->used += size + pad;
  return ptr + pad;
};
void region_free(Region *region) {
  free(region->data);
  region->used = 0;
}

// string view
typedef struct {
  char *cstr;
  size_t len;
} String;
String String_cstr(char *cstr) {
  return (String){.cstr = cstr, .len = strlen(cstr)};
}
void String_print(String str) { printf("%.*s", (int)str.len, str.cstr); }
String String_concat(String a, String b, Region *region) {
  char *buffer = region_alloc(a.len + b.len + 1, region);
  if (buffer == NULL) {
    return (String){.cstr = buffer, .len = 0};
  }
  memcpy(buffer, a.cstr, a.len);
  memcpy(buffer + a.len, b.cstr, b.len);
  return (String){.cstr = buffer, .len = a.len + b.len};
}
int String_equal(String a, String b) {
  if (a.len != b.len)
    return 0;
  return strncmp(a.cstr, b.cstr, a.len) == 0;
}
int String_cmp(String a, String b) {
  int min_strlen = (int)(a.len < b.len ? a.len : b.len);
  int res = strncmp(a.cstr, b.cstr, (size_t)min_strlen);
  if (res == 0) {
    return (int)(a.len - b.len);
  } else {
    return res;
  }
}

// tokenizer
typedef struct Token Token;
typedef enum {
  TOKEN_INT,
  TOKEN_STR,
  TOKEN_FLOAT,
  TOKEN_OPERATOR,
  TOKEN_IDENTIFIER,
  TOKEN_PAREN,
  TOKEN_CNT,
} Token_Type;
struct Token {
  Token_Type toktyp;
  int64_t ival;
  double fval;
  String str;
  Token *next;
};
int isoperator(char c) {
  if (c == '+' || c == '-' || c == '*' || c == '/' || c == '=' || c == '>' ||
      c == '<' || c == '!' || c == ',')
    return 1;
  return 0;
}
const char *TOKEN_tocstr(Token_Type toktyp) {
  switch (toktyp) {
  case TOKEN_INT:
    return "TOKEN_INT";
    break;
  case TOKEN_STR:
    return "TOKEN_STR";
    break;
  case TOKEN_FLOAT:
    return "TOKEN_FLOAT";
    break;
  case TOKEN_OPERATOR:
    return "TOKEN_OPERATOR";
    break;
  case TOKEN_IDENTIFIER:
    return "TOKEN_IDENTIFIER";
    break;
  case TOKEN_PAREN:
    return "TOKEN_PAREN";
    break;
  }
  return "TOKEN_UNKNOWN";
}
Token *tokenize(char *str, Region *region) {
  Token *root = region_alloc(sizeof(Token), region);
  Token *cur_tok = root;
  while (*str) {
    if (isspace(*str)) {
      str++;
      continue;
    }

    char *current = str;

    // match number
    char *dot = {0};
    if (isdigit(*current)) {
      while (*current) {
        current++;
        if (*current == '.') {
          dot = current;
          current++;
          if (!isdigit(*current)) {
            printf("[TOKENIZATION ERROR]: float malformed\n");
            return NULL;
          }
        }
        if (!isdigit(*current)) {
          break;
        }
      }
      // printf("%.*s\n", current - str, str);
      Token *tok = region_alloc(sizeof(Token), region);
      tok->str = (String){.cstr = str, .len = (unsigned long)(current - str)};
      if (dot) {
        tok->fval = atof(str);
        tok->toktyp = TOKEN_FLOAT;
      } else {
        tok->ival = atoll(str);
        tok->fval = (double)tok->ival;
        tok->toktyp = TOKEN_INT;
      }
      cur_tok->next = tok;
      cur_tok = cur_tok->next;
      str += current - str;
      continue;
    }

    //

    // match string literal
    if (*current == '"') {
      while (*current) {
        current++;
        if (*current == '"') {
          break;
        }
      }
      if (*current == 0) {
        printf("[TOKENIZATION ERROR]: quote not closing\n");
        return NULL;
      }
      current++; // skip right quotation
      Token *tok = region_alloc(sizeof(Token), region);
      tok->str =
          (String){.cstr = str + 1, .len = (unsigned long)(current - str - 2)};
      // printf("%.*s\n", current - str, str);
      tok->toktyp = TOKEN_STR;
      cur_tok->next = tok;
      cur_tok = cur_tok->next;
      str += current - str;
      continue;
    }

    // match operator
    if (isoperator(*current)) {
      current++; // skip one character
      if (*current == '=') {
        current++;
      }
      Token *tok = region_alloc(sizeof(Token), region);
      tok->str = (String){.cstr = str, .len = (unsigned long)(current - str)};
      // printf("%.*s\n", current - str, str);
      tok->toktyp = TOKEN_OPERATOR;
      cur_tok->next = tok;
      cur_tok = cur_tok->next;
      str += current - str;
      continue;
    }

    // match ()
    if (*current == '(' || *current == ')') {
      current++; // skip one character
      Token *tok = region_alloc(sizeof(Token), region);
      tok->str = (String){.cstr = str, .len = (unsigned long)(current - str)};
      // printf("%.*s\n", current - str, str);
      tok->toktyp = TOKEN_PAREN;
      cur_tok->next = tok;
      cur_tok = cur_tok->next;
      str += current - str;
      continue;
    }

    // match identifier (start with an word)
    if ((*current >= 'a' && *current <= 'z') ||
        (*current >= 'A' && *current <= 'Z')) {
      while (*current) {
        current++;
        if ((ispunct(*current) && *current != '_') || isspace(*current) ||
            *current == 0)
          break;
      }
      Token *tok = region_alloc(sizeof(Token), region);
      tok->str = (String){.cstr = str, .len = (unsigned long)(current - str)};
      tok->toktyp = TOKEN_IDENTIFIER;
      cur_tok->next = tok;
      cur_tok = cur_tok->next;
      str += current - str;
      continue;
    }
    printf("[TOKENIZATION ERROR]: unknown token: \"%s\"\n", str);
    return NULL;
  }
  return root->next;
}
void Token_print(Token *root) {
  if (root == NULL) {
    return;
  }
  String_print(root->str);
  printf(" -> %s\n", TOKEN_tocstr(root->toktyp));
  Token_print(root->next);
}

// abstract syntax tree
typedef struct Node Node;
typedef enum {
  NODE_NIL,
  NODE_INT,
  NODE_FLOAT,
  NODE_STR,
  NODE_IDENTIFIER,
  // operator
  NODE_ADD,
  NODE_SUB,
  NODE_DIV,
  NODE_MUL,
  NODE_ASSIGN,
  NODE_COMMA,
  NODE_EQ,
  NODE_GT,
  NODE_LT,
  NODE_NOT,
  NODE_NEQ,
  NODE_GEQ,
  NODE_LEQ,

  // parentheses
  NODE_LPAREN,
  NODE_RPAREN,
  NODE_CNT,
} Node_Type;
struct Node {
  Node_Type ndtyp;
  Token *token;
  Node *lhs;
  Node *rhs;
};
Node *Node_token(Token *token, Region *region) {
  Node *node = region_alloc(sizeof(Node), region);
  Token *persist_token = region_alloc(sizeof(Token), region);
  memcpy(persist_token, token, sizeof(Token));
  char *buffer = region_alloc(token->str.len + 1, region);
  memcpy(buffer, token->str.cstr, token->str.len);
  persist_token->str = (String){.cstr = buffer, .len = token->str.len};
  node->token = persist_token;
  switch (token->toktyp) {
  case TOKEN_INT:
    node->ndtyp = NODE_INT;
    break;
  case TOKEN_FLOAT:
    node->ndtyp = NODE_FLOAT;
    break;
  case TOKEN_STR:
    node->ndtyp = NODE_STR;
    break;
  case TOKEN_IDENTIFIER:
    node->ndtyp = NODE_IDENTIFIER;
    break;
  case TOKEN_OPERATOR:
    if (String_equal(token->str, (String){.cstr = "+", .len = 1}))
      node->ndtyp = NODE_ADD;
    if (String_equal(token->str, (String){.cstr = "-", .len = 1}))
      node->ndtyp = NODE_SUB;
    if (String_equal(token->str, (String){.cstr = "*", .len = 1}))
      node->ndtyp = NODE_MUL;
    if (String_equal(token->str, (String){.cstr = "/", .len = 1}))
      node->ndtyp = NODE_DIV;
    if (String_equal(token->str, (String){.cstr = "=", .len = 1}))
      node->ndtyp = NODE_ASSIGN;
    if (String_equal(token->str, (String){.cstr = ",", .len = 1}))
      node->ndtyp = NODE_COMMA;
    if (String_equal(token->str, (String){.cstr = "==", .len = 2}))
      node->ndtyp = NODE_EQ;
    if (String_equal(token->str, (String){.cstr = ">", .len = 1}))
      node->ndtyp = NODE_GT;
    if (String_equal(token->str, (String){.cstr = "<", .len = 1}))
      node->ndtyp = NODE_LT;
    if (String_equal(token->str, (String){.cstr = "!", .len = 1}))
      node->ndtyp = NODE_NOT;
    if (String_equal(token->str, (String){.cstr = "!=", .len = 2}))
      node->ndtyp = NODE_NEQ;
    if (String_equal(token->str, (String){.cstr = ">=", .len = 2}))
      node->ndtyp = NODE_GEQ;
    if (String_equal(token->str, (String){.cstr = "<=", .len = 2}))
      node->ndtyp = NODE_LEQ;
    break;
  case TOKEN_PAREN:
    if (strncmp(token->str.cstr, "(", token->str.len) == 0)
      node->ndtyp = NODE_LPAREN;
    if (strncmp(token->str.cstr, ")", token->str.len) == 0)
      node->ndtyp = NODE_RPAREN;
    break;
  }
  return node;
}
const char *Node_tocstr(Node *node) {
  switch (node->ndtyp) {
  case NODE_NIL:
    return "NODE_NIL";
    break;
  case NODE_INT:
    return "NODE_INT";
    break;
  case NODE_FLOAT:
    return "NODE_FLOAT";
    break;
  case NODE_STR:
    return "NODE_STR";
    break;
  case NODE_IDENTIFIER:
    return "NODE_IDENTIFIER";
    break;
  case NODE_ADD:
    return "NODE_ADD";
    break;
  case NODE_SUB:
    return "NODE_SUB";
    break;
  case NODE_DIV:
    return "NODE_DIV";
    break;
  case NODE_MUL:
    return "NODE_MUL";
    break;
  case NODE_ASSIGN:
    return "NODE_ASSIGN";
    break;
  case NODE_COMMA:
    return "NODE_COMMA";
    break;
  case NODE_EQ:
    return "NODE_EQ";
    break;
  case NODE_GT:
    return "NODE_GT";
    break;
  case NODE_LT:
    return "NODE_LT";
    break;
  case NODE_NOT:
    return "NODE_NOT";
    break;
  case NODE_NEQ:
    return "NODE_NEQ";
    break;
  case NODE_GEQ:
    return "NODE_GEQ";
    break;
  case NODE_LEQ:
    return "NODE_LEQ";
    break;
  }
  return NULL;
}
typedef struct {
  int lhs;
  int rhs;
} Bind;
Bind Node_bindpw(Node *node) {
  switch (node->ndtyp) {
  case NODE_ADD:
  case NODE_SUB:
    return (Bind){.lhs = 5, .rhs = 6};
    break;
  case NODE_MUL:
  case NODE_DIV:
    return (Bind){.lhs = 7, .rhs = 8};
    break;
  case NODE_ASSIGN:
    return (Bind){
        .lhs = 4,
        .rhs = 3}; // this should be right associative in order to group expr on
                   // the lhs which means weaker binding power on lhs
  case NODE_COMMA:
    return (Bind){.lhs = 1, .rhs = 2};
  case NODE_GEQ:
  case NODE_LEQ:
  case NODE_GT:
  case NODE_LT:
  case NODE_EQ:
  case NODE_NEQ:
    return (Bind){.lhs = 9, .rhs = 10};
  }
  return (Bind){.lhs = 0, .rhs = 0};
}
void Node_print(Node *root, int depth) {
  if (root == NULL)
    return;
  printf("|%*s|%.*s\n", depth, " ", (int)root->token->str.len,
         root->token->str.cstr);
  Node_print(root->lhs, depth + 1);
  Node_print(root->rhs, depth + 1);
}
void Node_print_detail(Node *root, int depth) {
  if (root == NULL)
    return;
  printf("|%*s|%.*s -> %s\n", depth, " ", (int)root->token->str.len,
         root->token->str.cstr, Node_tocstr(root));
  Node_print_detail(root->lhs, depth + 1);
  Node_print_detail(root->rhs, depth + 1);
}
Node *pratt_parsing(Token **tokroot, Region *region, int min_bp) {
  if (*tokroot == NULL) {
    printf("[PARSING ERROR]: value expected, get NULL\n");
    return NULL;
  }
  Node *lhs = Node_token(*tokroot, region);
  if (lhs->ndtyp == NODE_INT || lhs->ndtyp == NODE_FLOAT ||
      lhs->ndtyp == NODE_STR || lhs->ndtyp == NODE_IDENTIFIER) {
    // consume its token if its a value
    *tokroot = (*tokroot)->next;
  } else if (lhs->ndtyp == NODE_LPAREN) {
    // consume ( token
    *tokroot = (*tokroot)->next;
    lhs = pratt_parsing(tokroot, region, 0);
    if (lhs == NULL) {
      printf("[PARSING ERROR]: expr expected in parenthese, get NULL\n");
      return NULL;
    }
    Node *rparen = Node_token(*tokroot, region);
    if (rparen->ndtyp != NODE_RPAREN) {
      printf("[PARSING ERROR]: ) expected, get \"%.*s\"\n",
             (int)rparen->token->str.len, rparen->token->str.cstr);
      return NULL;
    }
    *tokroot = (*tokroot)->next; // consume ) token
  } else if (lhs->ndtyp == NODE_ADD || lhs->ndtyp == NODE_SUB ||
             lhs->ndtyp == NODE_NOT) {
    // get an operator when there should be a value, it might be unary operator
    *tokroot = (*tokroot)->next; // consume prefix token
                                 // special case if nested prefix
    Node *tmp = lhs;
    // consume all prefix
    while (strncmp((*tokroot)->str.cstr, "+", (*tokroot)->str.len) == 0 ||
           strncmp((*tokroot)->str.cstr, "-", (*tokroot)->str.len) == 0 ||
           strncmp((*tokroot)->str.cstr, "!", (*tokroot)->str.len) == 0) {
      Node *node = Node_token(*tokroot, region);
      tmp->rhs = node;
      tmp = tmp->rhs;
      *tokroot = (*tokroot)->next;
    }
    if ((*tokroot)->toktyp == TOKEN_FLOAT || (*tokroot)->toktyp == TOKEN_INT ||
        (*tokroot)->toktyp == TOKEN_STR) {
      tmp->rhs = Node_token(*tokroot, region);
      *tokroot = (*tokroot)->next;
    } else if ((*tokroot)->toktyp == TOKEN_PAREN &&
               strncmp((*tokroot)->str.cstr, "(", (*tokroot)->str.len) == 0) {
      *tokroot = (*tokroot)->next;
      tmp->rhs = pratt_parsing(tokroot, region, 0);
      if (tmp->rhs == NULL) {
        printf("[PARSING ERROR]: expr expected in parenthese, get NULL\n");
        return NULL;
      }
      Node *rparen = Node_token(*tokroot, region);
      if (rparen->ndtyp != NODE_RPAREN) {
        printf("[PARSING ERROR]: ) expected, get \"%.*s\"\n",
               (int)rparen->token->str.len, rparen->token->str.cstr);
        return NULL;
      }
      *tokroot = (*tokroot)->next; // consume ) token
    }
    //
  } else {
    printf("[PARSING ERROR]: value expected, get \"%.*s\"\n",
           (int)lhs->token->str.len, lhs->token->str.cstr);
    return NULL;
  }
  while (*tokroot != NULL) {
    //
    Node *operator = Node_token(*tokroot, region);
    if (operator->ndtyp == NODE_RPAREN) {
      break;
    }
    if (operator->token->toktyp == TOKEN_OPERATOR) {
    } else {
      printf("[PARSING ERROR]: operator expected, get \"%.*s\"\n",
             (int)operator->token->str.len, operator->token->str.cstr);
      return NULL;
    }
    int l_bp = Node_bindpw(operator).lhs;
    int r_bp = Node_bindpw(operator).rhs;
    if (l_bp < min_bp) {
      return lhs;
    }
    // consume the operator
    *tokroot = (*tokroot)->next;
    operator->rhs = pratt_parsing(tokroot, region, r_bp);
    if (operator->rhs == NULL) {
      printf("[PARSING ERROR]: expr expected, get NULL\n");
      return NULL;
    }
    operator->lhs = lhs;
    lhs = operator;
    //
  }

  return lhs;
}

// hashmap
unsigned long hash(String str) {
  unsigned long hash = 5381;
  unsigned long c;

  for (size_t i = 0; i < str.len; i++) {
    c = (unsigned long)*(str.cstr++);
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }
  return hash;
}
#define HASH_MAP_BUCKET_SIZE 1024
typedef enum {
  VAR_TYPE_INT,
  VAR_TYPE_FLOAT,
  VAR_TYPE_STR,
  VAR_TYPE_CNT
} Var_Type;
typedef struct {
  String var;
  int occupied;
  Node typ_n_val;
} Var_Mapping;
typedef struct {
  Var_Mapping *items;
  size_t used;
  size_t size;
} Hashmap;
void Hashmap_init(Hashmap *hm, Region *region) {
  if (hm == NULL)
    return;
  hm->items = region_alloc(sizeof(Var_Mapping) * HASH_MAP_BUCKET_SIZE, region);
  hm->used = 0;
  hm->size = HASH_MAP_BUCKET_SIZE;
}
int Hashmap_append(Hashmap *hm, String var, Node val) {
  if (hm == NULL) {
    printf("[HASHMAP ERROR]: hashmap is NULL\n");
    return -1;
  }
  unsigned long idx = hash(var) % hm->size;
  for (size_t i = 0; i < hm->size && hm->items[idx].occupied; i++) {
    if (String_equal(hm->items[idx].var, var)) {
      hm->items[idx].typ_n_val = val;
      hm->items[idx].var = var;
      hm->items[idx].occupied = 1;
      return (int)idx;
    }
    idx = (idx + 1) % hm->size;
  }
  if (hm->used >= HASH_MAP_BUCKET_SIZE / 2) {
    printf("[HASHMAP ERROR]: hashmap full\n");
    return -1;
  }
  hm->items[idx].typ_n_val = val;
  hm->items[idx].var = var;
  hm->items[idx].occupied = 1;
  hm->used++;
  return (int)idx;
}
Node *Hashmap_find(Hashmap *hm, Region *region, String var) {
  Node *node = region_alloc(sizeof(Node), region);
  node->token = region_alloc(sizeof(Token), region);
  if (hm == NULL) {
    printf("[HASHMAP ERROR]: hashmap is NULL\n");
    return node;
  }
  unsigned long idx = hash(var) % hm->size;
  for (size_t i = 0; i < hm->size && hm->items[idx].occupied; i++) {
    if (String_equal(hm->items[idx].var, var)) {
      return &hm->items[idx].typ_n_val;
    }
    idx = (idx + 1L) % hm->size;
  }
  printf("[HASHMAP ERROR]: var not found\n");
  return node;
}

Node *Node_eval(Node *root, Region *region, Hashmap *hm) {
  if (root == NULL)
    return NULL;
  switch (root->ndtyp) {
  case NODE_NOT: {
    Node *node = region_alloc(sizeof(Node), region);
    node->token = region_alloc(sizeof(Token), region);
    node->ndtyp = NODE_INT;
    if (root->lhs == NULL) {
      Node *val_node = Node_eval(root->rhs, region, hm);
      if (val_node == NULL) {
        return NULL;
      }
      double fval = !val_node->token->fval;
      node->token->fval = fval;
      if (val_node->ndtyp == NODE_INT) {
        node->ndtyp = NODE_INT;
        int64_t ival = !val_node->token->ival;
        node->token->ival = ival;
        return node;
      }
      return node;
    }
    return node;
  } break;

    //
    //
  case NODE_NEQ: {
    Node *node = region_alloc(sizeof(Node), region);
    node->token = region_alloc(sizeof(Token), region);
    node->ndtyp = NODE_INT;
    Node *lhs_node = Node_eval(root->lhs, region, hm);
    Node *rhs_node = Node_eval(root->rhs, region, hm);
    if (lhs_node->ndtyp == NODE_STR && rhs_node->ndtyp == NODE_STR) {
      node->token->ival =
          !String_equal(lhs_node->token->str, rhs_node->token->str);
      node->token->fval = (double)node->token->ival;
      return node;
    }
    if ((lhs_node->ndtyp == NODE_INT || lhs_node->ndtyp == NODE_FLOAT) &&
        (rhs_node->ndtyp == NODE_FLOAT || rhs_node->ndtyp == NODE_INT)) {
      node->token->ival = lhs_node->token->fval != rhs_node->token->fval;
      node->token->fval = (double)node->token->ival;
      return node;
    }
    return node;
  } break;

    //
    //
  case NODE_LEQ: {
    Node *node = region_alloc(sizeof(Node), region);
    node->token = region_alloc(sizeof(Token), region);
    node->ndtyp = NODE_INT;
    Node *lhs_node = Node_eval(root->lhs, region, hm);
    Node *rhs_node = Node_eval(root->rhs, region, hm);
    if (lhs_node->ndtyp == NODE_STR && rhs_node->ndtyp == NODE_STR) {
      node->token->ival =
          String_cmp(lhs_node->token->str, rhs_node->token->str) <= 0;
      node->token->fval = (double)node->token->ival;
      return node;
    }
    if ((lhs_node->ndtyp == NODE_INT || lhs_node->ndtyp == NODE_FLOAT) &&
        (rhs_node->ndtyp == NODE_FLOAT || rhs_node->ndtyp == NODE_INT)) {
      node->token->ival = lhs_node->token->fval <= rhs_node->token->fval;
      node->token->fval = (double)node->token->ival;
      return node;
    }
    return node;
  } break;

    //
    //
  case NODE_LT: {
    Node *node = region_alloc(sizeof(Node), region);
    node->token = region_alloc(sizeof(Token), region);
    node->ndtyp = NODE_INT;
    Node *lhs_node = Node_eval(root->lhs, region, hm);
    Node *rhs_node = Node_eval(root->rhs, region, hm);
    if (lhs_node->ndtyp == NODE_STR && rhs_node->ndtyp == NODE_STR) {
      node->token->ival =
          String_cmp(lhs_node->token->str, rhs_node->token->str) < 0;
      node->token->fval = (double)node->token->ival;
      return node;
    }
    if ((lhs_node->ndtyp == NODE_INT || lhs_node->ndtyp == NODE_FLOAT) &&
        (rhs_node->ndtyp == NODE_FLOAT || rhs_node->ndtyp == NODE_INT)) {
      node->token->ival = lhs_node->token->fval < rhs_node->token->fval;
      node->token->fval = (double)node->token->ival;
      return node;
    }
    return node;
  } break;

    //
    //
  case NODE_GEQ: {
    Node *node = region_alloc(sizeof(Node), region);
    node->token = region_alloc(sizeof(Token), region);
    node->ndtyp = NODE_INT;
    Node *lhs_node = Node_eval(root->lhs, region, hm);
    Node *rhs_node = Node_eval(root->rhs, region, hm);
    if (lhs_node->ndtyp == NODE_STR && rhs_node->ndtyp == NODE_STR) {
      node->token->ival =
          String_cmp(lhs_node->token->str, rhs_node->token->str) >= 0;
      node->token->fval = (double)node->token->ival;
      return node;
    }
    if ((lhs_node->ndtyp == NODE_INT || lhs_node->ndtyp == NODE_FLOAT) &&
        (rhs_node->ndtyp == NODE_FLOAT || rhs_node->ndtyp == NODE_INT)) {
      node->token->ival = lhs_node->token->fval >= rhs_node->token->fval;
      node->token->fval = (double)node->token->ival;
      return node;
    }
    return node;
  } break;

    //
    //
  case NODE_GT: {
    Node *node = region_alloc(sizeof(Node), region);
    node->token = region_alloc(sizeof(Token), region);
    node->ndtyp = NODE_INT;
    Node *lhs_node = Node_eval(root->lhs, region, hm);
    Node *rhs_node = Node_eval(root->rhs, region, hm);
    if (lhs_node->ndtyp == NODE_STR && rhs_node->ndtyp == NODE_STR) {
      node->token->ival =
          String_cmp(lhs_node->token->str, rhs_node->token->str) > 0;
      node->token->fval = (double)node->token->ival;
      return node;
    }
    if ((lhs_node->ndtyp == NODE_INT || lhs_node->ndtyp == NODE_FLOAT) &&
        (rhs_node->ndtyp == NODE_FLOAT || rhs_node->ndtyp == NODE_INT)) {
      node->token->ival = lhs_node->token->fval > rhs_node->token->fval;
      node->token->fval = (double)node->token->ival;
      return node;
    }
    return node;
  } break;

    //
    //
  case NODE_EQ: {
    Node *node = region_alloc(sizeof(Node), region);
    node->token = region_alloc(sizeof(Token), region);
    node->ndtyp = NODE_INT;
    Node *lhs_node = Node_eval(root->lhs, region, hm);
    Node *rhs_node = Node_eval(root->rhs, region, hm);
    if (lhs_node->ndtyp == NODE_STR && rhs_node->ndtyp == NODE_STR) {
      node->token->ival =
          String_equal(lhs_node->token->str, rhs_node->token->str);
      node->token->fval = (double)node->token->ival;
      return node;
    }
    if ((lhs_node->ndtyp == NODE_INT || lhs_node->ndtyp == NODE_FLOAT) &&
        (rhs_node->ndtyp == NODE_FLOAT || rhs_node->ndtyp == NODE_INT)) {
      node->token->ival = lhs_node->token->fval == rhs_node->token->fval;
      node->token->fval = (double)node->token->ival;
      return node;
    }
    return node;
  } break;

    //
    //
  case NODE_INT:
  case NODE_FLOAT:
  case NODE_STR: {
    return root;
  } break;
    //
    //

  case NODE_ADD: {
    Node *node = region_alloc(sizeof(Node), region);
    node->token = region_alloc(sizeof(Token), region);
    node->ndtyp = NODE_FLOAT;
    if (root->lhs == NULL) {
      Node *val_node = Node_eval(root->rhs, region, hm);
      if (val_node == NULL) {
        return NULL;
      }
      double fval = val_node->token->fval;
      node->token->fval = fval;
      if (val_node->ndtyp == NODE_INT) {
        node->ndtyp = NODE_INT;
        int64_t ival = val_node->token->ival;
        node->token->ival = ival;
        return node;
      }
      return node;
    }
    double lhs = Node_eval(root->lhs, region, hm) == NULL
                     ? NAN
                     : Node_eval(root->lhs, region, hm)->token->fval;
    double rhs = Node_eval(root->rhs, region, hm) == NULL
                     ? NAN
                     : Node_eval(root->rhs, region, hm)->token->fval;
    node->token->fval = lhs + rhs;
    Node *lhs_node = Node_eval(root->lhs, region, hm);
    Node *rhs_node = Node_eval(root->rhs, region, hm);
    if (lhs_node->ndtyp == NODE_STR && rhs_node->ndtyp == NODE_STR) {
      Node *node = region_alloc(sizeof(Node), region);
      node->token = region_alloc(sizeof(Token), region);
      node->ndtyp = NODE_STR;
      node->token->str =
          String_concat(lhs_node->token->str, rhs_node->token->str, region);
      return node;
    }
    if (lhs_node->ndtyp == NODE_INT && rhs_node->ndtyp == NODE_INT) {
      node->ndtyp = NODE_INT;
      node->token->ival = lhs_node->token->ival + rhs_node->token->ival;
      node->token->fval = (double)node->token->ival;
      return node;
    }
    return node;
  } break;

    //
    //
  case NODE_MUL: {
    Node *node = region_alloc(sizeof(Node), region);
    node->token = region_alloc(sizeof(Token), region);
    node->ndtyp = NODE_FLOAT;
    double lhs = Node_eval(root->lhs, region, hm) == NULL
                     ? NAN
                     : Node_eval(root->lhs, region, hm)->token->fval;
    double rhs = Node_eval(root->rhs, region, hm) == NULL
                     ? NAN
                     : Node_eval(root->rhs, region, hm)->token->fval;
    node->token->fval = lhs * rhs;
    Node *lhs_node = Node_eval(root->lhs, region, hm);
    Node *rhs_node = Node_eval(root->rhs, region, hm);
    if (lhs_node->ndtyp == NODE_STR || rhs_node->ndtyp == NODE_STR) {
      printf("[EVALUATION ERROR]: string does not support this "
             "operator\n");
      return NULL;
    }
    if (lhs_node->ndtyp == NODE_INT && rhs_node->ndtyp == NODE_INT) {
      node->ndtyp = NODE_INT;
      node->token->ival = lhs_node->token->ival * rhs_node->token->ival;
      return node;
    }
    return node;
  } break;

    //
    //
  case NODE_DIV: {
    Node *node = region_alloc(sizeof(Node), region);
    node->token = region_alloc(sizeof(Token), region);
    node->ndtyp = NODE_FLOAT;
    double lhs = Node_eval(root->lhs, region, hm) == NULL
                     ? NAN
                     : Node_eval(root->lhs, region, hm)->token->fval;
    double rhs = Node_eval(root->rhs, region, hm) == NULL
                     ? NAN
                     : Node_eval(root->rhs, region, hm)->token->fval;
    node->token->fval = lhs / rhs;
    Node *lhs_node = Node_eval(root->lhs, region, hm);
    Node *rhs_node = Node_eval(root->rhs, region, hm);
    if (lhs_node->ndtyp == NODE_STR || rhs_node->ndtyp == NODE_STR) {
      printf("[EVALUATION ERROR]: string does not support this "
             "operator\n");
      return NULL;
    }
    if (lhs_node->ndtyp == NODE_INT && rhs_node->ndtyp == NODE_INT) {
      node->ndtyp = NODE_INT;
      if (rhs_node->token->ival == 0) {
        node->ndtyp = NODE_FLOAT;
        return node;
      }
      node->token->ival = lhs_node->token->ival / rhs_node->token->ival;
      return node;
    }
    return node;
  } break;

    //
    //
  case NODE_SUB: {
    Node *node = region_alloc(sizeof(Node), region);
    node->token = region_alloc(sizeof(Token), region);
    node->ndtyp = NODE_FLOAT;
    if (root->lhs == NULL) {
      Node *val_node = Node_eval(root->rhs, region, hm);
      node->token->fval = -val_node->token->fval;
      if (val_node->ndtyp == NODE_INT) {
        node->ndtyp = NODE_INT;
        int64_t ival = val_node->token->ival;
        node->token->ival = -ival;
        return node;
      }
      return node;
    }
    double lhs = Node_eval(root->lhs, region, hm) == NULL
                     ? NAN
                     : Node_eval(root->lhs, region, hm)->token->fval;
    double rhs = Node_eval(root->rhs, region, hm) == NULL
                     ? NAN
                     : Node_eval(root->rhs, region, hm)->token->fval;
    node->token->fval = lhs - rhs;
    Node *lhs_node = Node_eval(root->lhs, region, hm);
    Node *rhs_node = Node_eval(root->rhs, region, hm);
    if (lhs_node->ndtyp == NODE_STR || rhs_node->ndtyp == NODE_STR) {
      printf("[EVALUATION ERROR]: string does not support this "
             "operator\n");
      return NULL;
    }
    if (lhs_node->ndtyp == NODE_INT && rhs_node->ndtyp == NODE_INT) {
      node->ndtyp = NODE_INT;
      node->token->ival = lhs_node->token->ival - rhs_node->token->ival;
      return node;
    }
    return node;
  } break;

    //
    //
  case NODE_ASSIGN: {
    if (root->lhs->ndtyp != NODE_IDENTIFIER) {
      printf("[EVALUATION ERROR]: not assignable\n");
      return NULL;
    }
    Node *rval = Node_eval(root->rhs, region, hm);
    Hashmap_append(hm, root->lhs->token->str,
                   rval == NULL ? (Node){.ndtyp = NODE_NIL} : *rval);
    return rval;
  } break;

    //
    //
  case NODE_IDENTIFIER: {
    return Hashmap_find(hm, region, root->token->str);
  } break;

    //
    //
  case NODE_COMMA: {
    return Node_eval(root->lhs, region, hm), Node_eval(root->rhs, region, hm);
  } break;

    //
  }

  //
  //
  return NULL;
}
void Node_eval_print(Node *node) {
  if (!node) {
    // printf("(nil)\n");
    return;
  }
  switch (node->ndtyp) {
  case NODE_INT:
    printf("%ld\n", node->token->ival);
    break;
  case NODE_FLOAT:
    printf("%lf\n", node->token->fval);
    break;
  case NODE_STR:
  case NODE_IDENTIFIER:
    printf("%.*s\n", (int)node->token->str.len, node->token->str.cstr);
    break;
  case NODE_NIL:
    printf("(nil)\n");
  }
}

int main(int argc, char **argv) {
  Region region = {0};
  region_init(&region);
  Hashmap hm = {0};
  Hashmap_init(&hm, &region);

  if (argc == 2) {
    Token *root = tokenize(argv[1], &region);
    // Token_print(root);
    Token *tmp = root;
    Node *ast = pratt_parsing(&tmp, &region, 0);
    Node_print(ast, 1);
    region_free(&region);
    return 0;
  }
  if (argc > 2) {
    return 1;
  }

  Token *root =
      tokenize("\"Welcome to this tree-walker style parser\"", &region);
  // Token_print(root);
  Token *tmp = root;
  Node *ast = pratt_parsing(&tmp, &region, 0);
  Node_eval_print(Node_eval(ast, &region, &hm));
  // Node_print(ast, 1);
  root = tokenize("d=1", &region);
  tmp = root;
  ast = pratt_parsing(&tmp, &region, 0);
  Node_eval(ast, &region, &hm);
  root = tokenize("l=2", &region);
  tmp = root;
  ast = pratt_parsing(&tmp, &region, 0);
  Node_eval(ast, &region, &hm);
  root = tokenize("t=3", &region);
  tmp = root;
  ast = pratt_parsing(&tmp, &region, 0);
  Node_eval(ast, &region, &hm);

  root = tokenize("a=(1+2)*-(3-(4*-(5-6)))--(-(7))", &region);
  tmp = root;
  ast = pratt_parsing(&tmp, &region, 0);
  // Node_print(ast, 1);
  Node_eval(ast, &region, &hm);
  if (ast != NULL) {
    Node_eval_print(Node_eval(ast, &region, &hm));
  }

  root = tokenize("1.0==1", &region);
  tmp = root;
  ast = pratt_parsing(&tmp, &region, 0);
  Node_print_detail(ast, 1);
  Node_eval(ast, &region, &hm);
  if (ast != NULL) {
    Node_eval_print(Node_eval(ast, &region, &hm));
  }

  char *command = NULL;
  while (1) {
    char *prompt = "> ";
    command = readline(prompt);
    if (!command) {
      break;
    }
    if (command && *command)
      add_history(command);
    if (strcasecmp(command, "exit") == 0 || strcasecmp(command, "quit") == 0) {
      free(command);
      break;
    }

    Token *root = tokenize(command, &region);
    Token_print(root);
    Node *ast = {0};
    if (root != NULL)
      ast = pratt_parsing(&root, &region, 0);
    Node_print_detail(ast, 1);
    if (ast != NULL) {
      Node_eval_print(Node_eval(ast, &region, &hm));
    }
    free(command);
    // printf("Mem region usage: %zu (bytes) %zu%%\n", region.used,
    //        region.used * 100 / (REIGON_INIT_SIZE));
    // region_free(&region);
    // region_init(&region);
  }
  region_free(&region);

  // for (int i = 'a'; i < 'z' + 1; i++) {
  //   char buf[2] = {0};
  //   buf[0] = i;
  //   printf("%s %ld\n", buf, hash((String){.cstr = buf, .len = 1}) % hm.size);
  // }
  return 0;
}
