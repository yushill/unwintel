# GDB init file for UnWinTel binary product (app) -*- Shell-script -*-
### Launch command
file app
set args ./app 1

### Function uwtenter: enters a uwt function call
define uwtenter
step
finish
step
end

### Break on each thread start
break UWT::Context::process

# Break into main
break main
run
