
#include "compiler_functions.h"


char* add_to_tos(int type, char *name ){
    /* verifying that ID does not already exist in the global context*/
    if(ht_search(glob_context, name) != NULL)
        return "\nErreur: Méthode déjà déclarée avec ce nom\n";

    /* Verifying that main is not declared with a return type other than void*/
    if(type != VOIDTYPE && !strcmp(name,"main"))
        return "\nErreur: La méthode main doit être de type void\n";

    if(!strcmp(name, next_label_name()))
        labelCount++;
    
    /* we store the method in the symbol table*/
    Ht_item *item = create_item(name, ID_METHOD, type);
    newname(item);

    /* The parameters declaration within a method has its specific context
    which can be overwritten by new declarations in the method block.
    */
    pushctx(CTX_METHOD);
    global_code[nextquad].label = malloc(strlen(name)+1);
    strcpy(global_code[nextquad].label,name);

    return NULL;
}

/* verifies that all the return types in the return list is of the same type as type*/ 
int verify_returns(list rtrn, int type){
  
  while(rtrn){
    if(global_code[rtrn->addr].op2.u.cst != type)
      return 0;
    rtrn = rtrn->suiv;
  }
  return 1;
}


char* end_func(char *name, int ctx_count, param p){

  if(!strcmp(name, "main")){
    if(p != NULL){
      return "\nErreur: Main ne prends pas de paramètres\n";
    }
    quadop qo; 
    qo.type = QO_CST; 
    qo.u.cst = 10; 
    gencode(qo,qo,qo, Q_SYSCALL, "end", -1, NULL);
  }

  else{
    quadop qo;
    qo.type = QO_CST;
    qo.u.cst = ctx_count;
    
    // Notice that we stored the number of variables to pop in qo.
    gencode(qo, qo, qo, Q_ENDFUNC, new_endfunc_label(name), -1, p);
  }
  
  return NULL;
}

char* field_declare(declaration *dec, int type){

  quadop qo,q1;
  declaration *pt = dec;
  qo.type = QO_GLOBAL;
  /* glob_id is a list of all arrays and global variables declared of same type, see rule below*/
  while(pt != NULL){

    /* verifying that the ID does not already exist in the current context*/
    if( ht_search(glob_context, pt->name) != NULL ) 
      return "\nErreur: Identificateur déjà déclaré\n";

    if(!strcmp(pt->name, next_label_name()))
      labelCount++;
    
    /* declaration of the global id in the quad */
    qo.u.global.name = malloc(strlen(pt->name + 1));
    strcpy(qo.u.global.name, pt->name);
    qo.u.global.size = pt->size;
    qo.u.global.type = pt->type;
    gencode(qo, qo, qo, Q_DECL, NULL, -1, NULL);

    /* we increment the global variables counter, useful for MIPS*/
    glob_dec_count++;

    /* we store the id in the global symbol table*/
    int id_type;
    if(pt->type == QO_TAB)
      id_type = ID_TAB;
    else if(pt->type == QO_SCAL)
      id_type = ID_VAR;
    Ht_item *item = create_item(pt->name, id_type, type);
    item->size = pt->size;
    newname(item);
    pt = pt->suiv;
	}

  return NULL;
}

char* var_declare(declaration *dec, int type){

  quadop qo;
  /* pointer to browse the list of IDs*/
  declaration *pt = dec;
  qo.type = QO_ID;
  
  while(pt != NULL){

    if( ht_search(curr_context, pt->name) != NULL ) 
      return "\nErreur: Identificateur déjà déclaré\n";
    
    if(!strcmp(pt->name, next_label_name()))
      labelCount++;

    qo.u.offset = 0;
    gencode(qo, qo, qo, Q_DECL, NULL, -1, NULL);

    Ht_item *item = create_item(pt->name, ID_VAR, type);
    newname(item);
    pt = pt->suiv;
  }

  return NULL;
}
                  
declaration get_declarations(char *name, declaration *next, int type, int size){
  declaration var;
  var.name = malloc((strlen(name)+1)); 
  strcpy(var.name, name);
  var.size = size;
  var.suiv = next;
  var.type = type;
  return var;
}

param push_param(char* name, int type, param next){

  Ht_item *item = create_item(name, ID_PARAM, type);
  newname(item);
  param p = (param) malloc(sizeof(struct param));
  p->type = type;
  p->next = next;
  return p;
}

char* verify_aff_types(int type1, int type2, int oper, Ht_item *item){
  
  if(item->value != type2)
    return "\nErreur: Type différents à l'affectation\n";

  if ((oper == Q_AFFADD || oper == Q_AFFSUB) && (item->value == BOOL || type2 == BOOL)) 
    return "\nErreur: Affectation += ou -= avec un type booléen\n";

  if(type1 == ID_TAB && item->id_type == ID_VAR) 
    return "\nErreur: Accès à l'indice d'une variable qui n'est pas un tableau\n";

  if( type1 == ID_VAR && item->id_type == ID_TAB) 
    return "\nErreur: Affectation scalaire comme tableau\n";

  return NULL;
}

void get_location(quadop* qo, quadop* q1, item_table* val, location l){

/* If location is an element in an array, we need to retrieve the offset from 
														the attribute 'quadop offset' and store it in q1. If not, we use q1 as an empty quadop
														in the AFF below.
													*/
  if(val->table == glob_context) {
    qo->type = QO_GLOBAL;
    qo->u.global.name = malloc(strlen(l.stringval)+1);
    strcpy(qo->u.global.name, l.stringval);
    qo->u.global.size = val->item->size;

    if(l.type == ID_VAR) {
      qo->u.global.type = QO_SCAL;
      q1->type = QO_EMPTY;
    }
    else if(l.type == ID_TAB) {
      qo->u.global.type = QO_TAB;
      *q1 = l.index;
    }
  }
  /* If location is a local variable, we retrieve the offset from TOS*/
  else {
    qo->type = QO_ID;
    qo->u.offset = offset(val);
  }
}

// if op1 is an element in array, we have the index in op3. otherwise, op3 is empty.
// this is done by get_location()
void bool_affectation(quadop op1, quadop op3, expr_val statement, expr_val expr){
  /*
    If the affectation is of bOOL type, we have to create two quads. One is for false affectation (0) and
    the other one for true (1). The rules on expr already generate incomplete GOTOs. At this stage we complete
    them with the address of the 2 quads we generated. The true attribute of expr is completed with the adress
    of the true affectation, and the false attribute is completed with the address of the false affectation.
  */

  /* True affectation */
  quadop op2;
  op2.type = QO_CST;
  op2.u.cst = true;
  complete(expr.t, nextquad);
  
  gencode(op1, op2, op3, Q_AFF, NULL, -1, NULL); 	

  /* To skip false affectation. Incomplete GOTO because we don't know yet where to skip.
    We add it to $$.next
  */
  op2.type = QO_EMPTY;
  statement.next = crelist(nextquad);
  gencode(op2, op2, op2, Q_GOTO, NULL, -1, NULL);

  /* False affectation*/
  op2.type = QO_CST;	
  op2.u.cst = false;		
  complete(expr.f, nextquad);											
  gencode(op1, op2, op3, Q_AFF, NULL, -1, NULL);	
}

char* for_declare(char* counter_id, expr_val expr1, expr_val expr2){

  /* The loop counter has its own context*/
  pushctx(CTX_FOR);

  /* Verifying types */
  if(expr1.type != INT || expr2.type != INT)
    return "\nErreur: le compteur de boucle doit être de type INT\n";

  /* To avoid having variables and labels holding the same name*/
  if(!strcmp(counter_id, next_label_name()))
    labelCount++;

  /* We declare the loop counter id, push it to the symbol table, and store the initial value expr1 in it*/
  quadop qo;
  qo.type = QO_ID;
  qo.u.offset = 0;
  gencode(qo, qo, qo, Q_DECL, NULL, -1, NULL);

  Ht_item *item = create_item(counter_id, ID_VAR, INT);
  newname(item);
  
  qo.type = QO_ID;
  qo.u.offset = 0;
  gencode(qo, expr1.result, expr2.result, Q_AFF, NULL, -1, NULL); 

  return NULL;
}

void gen_q_pop(int count){
  quadop q;
  q.type = QO_CST;
  q.u.cst = count;
  gencode(q, q, q, Q_POP, NULL, -1, NULL);
}

void gen_test_counter(char *counter_name, quadop q){
  item_table* val = lookup(counter_name);
  quadop qo;
  qo.type = QO_ID;
  qo.u.offset = offset(val);
  gencode(qo, qo, q, Q_GT, NULL, -1 , NULL);  
}

void gen_increment_and_loopback(int jump){
  /* gencode to increment the loop counter*/
  quadop qo, q1;
  qo.type = QO_ID;
  q1.type = QO_CST;
  q1.u.cst = 1;

  /* offset of id is 0 because it's the only variable in the context at this point */
  qo.u.offset = 0;
  gencode(qo, qo, q1, Q_ADD, NULL, -1, NULL);
  qo.type = QO_EMPTY;
  /* gencode to jump back to the Marker to test if the counter reached its max value */
  gencode(qo, qo, qo, Q_GOTO, NULL, jump, NULL);
}