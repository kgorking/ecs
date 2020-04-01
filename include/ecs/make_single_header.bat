@echo off

type contract.h > ecs_sh.h

echo. >> ecs_sh.h
type entity_id.h >> ecs_sh.h

echo. >> ecs_sh.h
type entity.h >> ecs_sh.h

echo. >> ecs_sh.h
type entity_range.h >> ecs_sh.h


echo. >> ecs_sh.h
type ..\..\threaded\threaded\threaded.h >> ecs_sh.h

echo. >> ecs_sh.h
type component_specifier.h >> ecs_sh.h

echo. >> ecs_sh.h
type component_pool_base.h >> ecs_sh.h

echo. >> ecs_sh.h
type component_pool.h >> ecs_sh.h

echo. >> ecs_sh.h
type system_verification.h >> ecs_sh.h

echo. >> ecs_sh.h
type system_base.h >> ecs_sh.h

echo. >> ecs_sh.h
type system.h >> ecs_sh.h

echo. >> ecs_sh.h
type context.h >> ecs_sh.h

echo. >> ecs_sh.h
type runtime.h >> ecs_sh.h
