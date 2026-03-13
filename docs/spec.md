# Abstack Language Specification v0.0.1
## 1. Goal
`abstack` is a declarative language made to define reusable docker builds using templates and services.
The compiler:
```
abstack file.abs -> Dockerfile(s)
```
Properties:
1. Output is always a Dockerfile by default
2. DSL focused on reuse and composability
3. *Not a programming language*
5. No loops, ifs or dynamic logic
## 2. File Structure
An `abstack` file contains:
```
template declarations
service declarations
```
Example:
```
template go_service(name, port) {
    ...
}

service api {
    use go_service("api", 8080)
}
```
## 3. Main Constructs
The language has only two type of top-level declarations
### Template
Defines a reusable pattern of a Docker build.
```
template go_service(name, port) {
    stage build {
        from "golang:1.22"
        run "go build"
    }
}
```
### Service
Defines a real container that will be compiler into a Dockerfile
```
service api {
    use go_service("api", 8080)
}
```
Each `service` generates:
```
Dockerfile.<service>
```
or
```
services/<service>/Dockerfile
```
TBD!
## 4. Template parameters
Templates accept parameters
```
template go_service(name, port)
```
Types supported in v0.0.1:
```
string
integer
identifier
```
Use inside a template:
```
run "go build./cmd/${name}
expose port
```
Interpolation use:
```
${variable}
```
## 5. Supported statements
Inside of `stage` or service.
### stage
Defines a docker stage.
```
stage build {
    ...
}
```
### from
Defines the base image.
```
from "golang:1.22"
```
equivalent:
```
FROM golang:1.22
```
### run
```
run "go build"
```
```
RUN go build
```
### copy
```
copy "." "/src"
```
```
COPY - /src
```
With stage:
```
copy from build "/src/app" "/app"
```
```
COPY --from=build /src/app /app
```
### env
```
env {
    KEY = "value"
    DEBUG = "true"
}
```
```
ENV KEY=value
ENV DEBUG=true
```
### expose
```
expose 8080
```
```
EXPOSE 8080
```
### cmd
```
cmd ["/app"]
```
```
CMD ["/app"]
```
### entrypoint
```
entrypoint ["/app"]
```
```
ENTRYPOINT ["/app"]
```
### use
Instantiates a template.
```
use go_service("api", 8080)
```
```
Expands a template within a service
```
## 6. Multi-stage builds
Templates can define multiple stages.
```
template go_service(name, port) {
    stage build {
        from "golang:1.22"
        run "go build"
    }

    stage runtime {
        from "alpine"
        copy from build "/src/app" "/app"
    }
}
```
```
FROM golang:1.22 AS build
RUN go build

FROM alpine AS runtime
COPY --from=build /src/app /app
```
## 7. Example of a complete file
```
template go_service(name, port) {

    stage build {
        from "golang:1.22-alpine"
        workdir "/src"

        copy "." "/src"

        run "go build -o app ./cmd/${name}"
    }

    stage runtime {
        from "alpine:3.19"

        copy from build "/src/app" "/app"

        expose port

        cmd ["/app"]
    }
}

service api {

    use go_service("api", 8080)

    env {
        SERVICE_NAME = "api"
        LOG_LEVEL = "info"
    }

}
```
## 8. Generation rules
Compilation:
```
service -> Dockerfile
```
Pipeline:
```
DSL
 ↓
AST
 ↓
semantic validation
 ↓
template expansion
 ↓
Docker IR
 ↓
Dockerfile
```
## 9. Validation rules
Errors that the compiler should be able to detect:
### Syntax
```
unexpected token @ line 10
missing brace @ line 3
invalid statement @ line 5
```
### Semantic
```
unknown template @ line 10
wrong number of arguments @ line 3
duplicate service @ line 4
duplicate stage name @ line 3
copy from unknown stage @ line 7
```
## 10. Explicit non-goals at (v0.0.1)
Not supported during this release:
```
imports
conditionals
loops
functions
variables outside templates
docker compose
kubernetes
policy rules
```
## 11. Grammar sketch
```
file
  = declaration*

declaration
  = template_decl
  | service_decl

template_decl
  = "template" IDENT "(" params ")" "{" template_body "}"

service_decl
  = "service" IDENT "{" service_body "}"

template_body
  = stage_decl*

service_body
  = use_stmt
  | env_block
  | stage_decl

stage_decl
  = "stage" IDENT "{" stage_stmt* "}"

stage_stmt
  = from_stmt
  | run_stmt
  | copy_stmt
  | workdir_stmt
  | expose_stmt
  | cmd_stmt
  | entrypoint_stmt

use_stmt
  = "use" IDENT "(" args ")"
```
## 12. File Extension
```
*.abstck
```
## 13. cli
```
abstack build file.abstck
abstack validate file.abstck
abstack dump-ast file.abstck
```
