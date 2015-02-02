
#ifndef TEMPLATE_FDW_BUILTINS
#define TEMPLATE_FDW_BUILTINS

#ifndef PGDLLEXPORT
#ifdef _MSC_VER
#define PGDLLEXPORT	__declspec(dllexport)

/*
 * PG_MODULE_MAGIC and PG_FUNCTION_INFO_V1 macros are broken for MSVC.
 * So, we redefine them.
 */

#undef PG_MODULE_MAGIC
#define PG_MODULE_MAGIC \
extern PGDLLEXPORT const Pg_magic_struct *PG_MAGIC_FUNCTION_NAME(void); \
const Pg_magic_struct * \
PG_MAGIC_FUNCTION_NAME(void) \
{ \
	static const Pg_magic_struct Pg_magic_data = PG_MODULE_MAGIC_DATA; \
	return &Pg_magic_data; \
} \
extern int no_such_variable

#undef PG_FUNCTION_INFO_V1
#define PG_FUNCTION_INFO_V1(funcname) \
extern PGDLLEXPORT const Pg_finfo_record * CppConcat(pg_finfo_,funcname)(void); \
const Pg_finfo_record * \
CppConcat(pg_finfo_,funcname) (void) \
{ \
	static const Pg_finfo_record my_finfo = { 1 }; \
	return &my_finfo; \
} \
extern int no_such_variable

#else
#define PGDLLEXPORT	PGDLLIMPORT
#endif
#endif

/*
 * Interface
 *
 */
extern PGDLLEXPORT Datum template_fdw_validator(PG_FUNCTION_ARGS);
extern PGDLLEXPORT Datum template_fdw_handler(PG_FUNCTION_ARGS);

#endif
