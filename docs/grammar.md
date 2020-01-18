## Falcon grammar

This document is a draft of the reference for the Falcon programming language grammar. It provides a formal definition 
of Falcon grammar. It is also not intended to be an introduction to the language concepts or standard library. **This 
document is a work in progress.**

The notation provided in this document is a dialect of the Extended Backus-Naur Form (EBNF). The postfix `*` means that 
the previous symbol or group may be repeated zero or more times. Similarly, the postfix `+` means that the preceding 
production should appear at least once. The postfix `?` is used for an optional production.

The grammar starts with the first rule that matches an Falcon program:

```
program -> decl* EOF ;
```

### Declarations

A program starts with a series of declarations:

```
decl -> class_decl 
     | function_decl 
     | variable_decl 
     | stmt ;

class_decl    -> "class" IDENTIFIER ( "<" IDENTIFIER )? "{" function* "}" ;
function_decl -> "fn" function ;
variable_decl -> "var" decl_list ";" ;
decl_list     -> single_decl ( "," single_decl )* ;
single_decl   -> IDENTIFIER ( "=" expr )? ;
```

### Statements

The remaining statement rules are:

```
stmt -> expr_stmt 
     | for_stmt
     | while_stmt
     | if_stmt
     | switch_stmt
     | break_stmt
     | next_stmt
     | return_stmt
     | block ;

expr_stmt   -> expr ";" ;
for_stmt    -> "for" single_decl "," expr "," expr block ;
while_stmt  -> "while" expr block ;
if_stmt     -> "if" expr block ( "else" ( block | if_stmt ) )? ;
switch_stmt -> "switch" expr "{" switch_case* else_case? "}" ;
switch_case -> "when" expr "->" stmt* ;
else_case   -> "else" "->" stmt* ;
break_stmt  -> "break" ";" ;
next_stmt   -> "next" ";" ;
return_stmt -> "return" expr? ";" ;
block       -> "{" decl* "}" ;
```

Note that `block` is a statement rule, but is also used as a non-terminal in another rule for function bodies. This 
means that a block can be used separately from a function declaration.

### Expressions

Falcon expressions produce values by using unary and binary operators with different levels of precedence. These 
precedence levels are explicit in the grammar by setting separate rules:

```
expr   -> assign ;
assign -> ( call "." )? IDENTIFIER ( "[" expr "]" )? "=" assign 
       | conditional ;

conditional -> logic_or ( "?" expr ":" conditional )? ;
logic_or    -> logic_and ( "or" logic_and )* ;
logic_and   -> equality ( "and" equality )* ;
equality    -> comparison ( ( "!=" | "==" ) comparison )* ;
comparison  -> addition ( ( ">" | ">=" | "<" | "<=" ) addition )* ;

addition       -> multiplication ( ( "-" | "+" ) multiplication )* ;
multiplication -> unary ( ( "/" | "*" | "%" ) unary )* ;
unary          -> ( "not" | "-" ) unary | exponent ;
exponent       -> "^" exponent | call ;

call    -> primary ( "(" args? ")" | "[" expr "]" | ( "." IDENTIFIER ) )* ;
primary -> "true" | "false" | "null" | "this" | NUMBER | STRING 
        | IDENTIFIER | "(" expr ")" | "[" args? "]" | "super" "." IDENTIFIER ;
```

### Recurrent rules

Some recurrent rules not defined in the sections above are:

```
function -> IDENTIFIER "(" params? ")" block ;
params   -> IDENTIFIER ( "," IDENTIFIER )* ;
args     -> expr ( "," expr )* ;
```

## Lexical Grammar

```
NUMBER     -> DIGIT+ ( '.' DIGIT+ )? ;
STRING     -> '"' non_double_quote* '"' ;
IDENTIFIER -> ALPHA ( ALPHA | DIGIT )* ;
ALPHA      -> 'a' ... 'z' | 'A' ... 'Z' | '_' ;
DIGIT      -> '0' ... '9' ;
```

where `non_double_quote` is is any single ASCII character aside from `"` (a double quote).
