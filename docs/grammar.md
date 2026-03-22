# Abstack Grammar Reference v0.2.0

Compact EBNF sketch for the current compiler behavior.

```ebnf
source            = { declaration } ;
declaration       = template_decl | service_decl ;

template_decl     = "template" identifier "(" [ param_list ] ")" "{" { stage_decl } "}" ;
service_decl      = "service" identifier "{" { service_stmt } "}" ;

param_list        = identifier { "," identifier } ;
arg_list          = value { "," value } ;

stage_decl        = "stage" identifier "{" { stage_stmt } "}" ;

stage_stmt        = from_stmt
                  | workdir_stmt
                  | copy_stmt
                  | run_stmt
                  | env_stmt
                  | expose_stmt
                  | cmd_stmt
                  | entrypoint_stmt ;

service_stmt      = use_stmt
                  | env_stmt
                  | expose_stmt
                  | cmd_stmt
                  | entrypoint_stmt
                  | port_stmt
                  | depends_on_stmt ;

use_stmt          = "use" identifier "(" [ arg_list ] ")" ;

from_stmt         = "from" value ;
workdir_stmt      = "workdir" value ;
copy_stmt         = "copy" [ "from" identifier ] value value ;
run_stmt          = "run" value ;
env_stmt          = "env" "{" { env_pair } "}" ;
env_pair          = identifier "=" value ;
expose_stmt       = "expose" value ;
cmd_stmt          = "cmd" value ;
entrypoint_stmt   = "entrypoint" value ;
port_stmt         = "port" value ;
depends_on_stmt   = "depends_on" identifier ;

value             = string | integer | identifier ;

identifier        = /[A-Za-z_][A-Za-z0-9_]*/ ;
string            = /"([^"\\]|\\.)*"/ ;
integer           = /[0-9]+/ ;
```

Notes:
1. Services are currently restricted semantically to exactly one `use` statement.
2. `identifier` values in templates are interpreted as parameter references.
3. String interpolation `${param}` is applied during template expansion.
