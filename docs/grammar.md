## YAPL grammar

This document is a draft of the reference for the YAPL programming language grammar. It provides a formal definition 
of YAPL grammar. It is also not intended to be an introduction to the language concepts or standard library. **This 
document is a work in progress.**

The notation provided in this document is a dialect of the Extended Backus-Naur Form (EBNF). The postfix `*` means that 
the previous symbol or group may be repeated zero or more times. Similarly, the postfix `+` means that the preceding 
production should appear at least once. The postfix `?` is used for an optional production.

The grammar starts with the first rule that matches an YAPL program:

```
program -> declaration* EOF ;
```

### Declarations

A program starts with a series of declarations:

```
declaration -> class_declaration 
            | function_declaration 
            | variable_declaration 
            | statement ;

class_declaration    -> "class" IDENTIFIER ( "<" IDENTIFIER )? "{" function* "}" ;
function_declaration -> "fun" function ;
variable_declaration -> "var" declaration_list ";" ;
declaration_list     -> single_declaration (  "," single_declaration )* ;
single_declaration   -> IDENTIFIER ( "=" expression )? ;
```

### Statements

The remaining statement rules are:

```
statement -> expression_statement 
          | for_statement
          | while_statement
          | if_statement
          | switch_statement
          | next_statement
          | return_statement
          | block ;

expression_statement -> expression ";" ;
for_statement        -> "for" ( variable_declaration | expression_statement | ";" )
                        expression? ";" expression? block ;
while_statement      -> "while" expression block ;
if_statement         -> ( "if" | "unless" ) expression block ( "else" ( block | if_statement ) )? ;
switch_statement     -> "switch" expression "{" switch_case* else_case? "}" ;
switch_case          -> "when" expression "->" statement* ;
else_case            -> "else" "->" statement* ;
next_statement       -> "next" ";" ;
return_statement     -> "return" expression? ";" ;
block                -> "{" declaration* "}" ;
```

Note that `block` is a statement rule, but is also used as a non-terminal in another rule for function bodies. This 
means that a block can be used separately from a function declaration.

### Expressions

YAPL expressions produce values by using unary and binary operators with different levels of precedence. These 
precedence levels are explicit in the grammar by setting separate rules:

```
expression -> assignment ;
assignment -> ( call "." )? IDENTIFIER "=" assignment
           | logic_or;

logic_or   -> logic_and ( "or" logic_and )* ;
logic_and  -> equality ( "and" equality )* ;
equality   -> comparison ( ( "!=" | "==" ) comparison )* ;
comparison -> addition ( ( ">" | ">=" | "<" | "<=" ) addition )* ;

addition       -> multiplication ( ( "-" | "+" ) multiplication )* ;
multiplication -> unary ( ( "/" | "*" | "%" ) unary )* ;

unary   -> ( "!" | "-" | "--" | "++" ) unary | call ;
call    -> primary ( "(" arguments? ")" | "." IDENTIFIER )* ;
primary -> "true" | "false" | "null" | "this" 
        | NUMBER | STRING | IDENTIFIER | "(" expression ")"
        | "super" "." IDENTIFIER ;
```

### Recurrent rules

Some recurrent rules not defined in the sections above are:

```
function   -> IDENTIFIER "(" parameters? ")" block ;
parameters -> IDENTIFIER ( "," IDENTIFIER )* ;
arguments  -> expression ( "," expression )* ;
```

## Lexical Grammar

```
NUMBER     -> DIGIT+ ( "." DIGIT+ )? ;
STRING     -> '"' non_double_quote* '"' ;
IDENTIFIER -> ALPHA ( ALPHA | DIGIT )* ;
ALPHA      -> 'a' ... 'z' | 'A' ... 'Z' | '_' ;
DIGIT      -> '0' ... '9' ;
```

where `non_double_quote` is is any single ASCII character aside from `"` (a double quote).
