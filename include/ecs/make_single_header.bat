@echo off
echo #include ^<algorithm^> > ecs_sh.h
echo #include ^<type_traits^> >> ecs_sh.h
echo #include ^<optional^> >> ecs_sh.h
echo #include ^<variant^> >> ecs_sh.h
echo #include ^<utility^> >> ecs_sh.h
echo #include ^<vector^> >> ecs_sh.h
echo #include ^<functional^> >> ecs_sh.h
echo #include ^<tuple^> >> ecs_sh.h
echo #include ^<map^> >> ecs_sh.h
echo #include ^<typeindex^> >> ecs_sh.h
echo #include ^<shared_mutex^> >> ecs_sh.h
echo #include ^<execution^> >> ecs_sh.h
echo. >> ecs_sh.h

echo // Contracts. If they are violated, the program is an invalid state, so nuke it from orbit >> ecs_sh.h
echo #define Expects(cond) ((cond) ? static_cast^<void^>(0) : std::terminate()) >> ecs_sh.h
echo #define Ensures(cond) ((cond) ? static_cast^<void^>(0) : std::terminate()) >> ecs_sh.h

echo. >> ecs_sh.h
echo. >> ecs_sh.h
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
type system.h >> ecs_sh.h

echo. >> ecs_sh.h
type system_impl.h >> ecs_sh.h

echo. >> ecs_sh.h
type context.h >> ecs_sh.h

echo. >> ecs_sh.h
type runtime.h >> ecs_sh.h
