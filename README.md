# template_fdw
PostgreSQL data wrapper for template tables - any DML and SELECT operations are disallowed

## The motivation:

<i>plpgsql_check</i> cannot verify queries over temporary tables that are created in plpgsql's function
runtime. For this use case it is necessary to create a fake temp table or disable <i>plpgsql_check</i> for this
function.

In reality temp tables are stored in own (per user) schema with higher priority than persistent
tables. So you can do (with following trick safetly):

    CREATE OR REPLACE FUNCTION public.disable_dml()
    RETURNS trigger
    LANGUAGE plpgsql AS $function$
    BEGIN
      RAISE EXCEPTION SQLSTATE '42P01'
         USING message = format('this instance of %I table doesn''t allow any DML operation', TG_TABLE_NAME),
               hint = format('you should to run "CREATE TEMP TABLE %1$I(LIKE %1$I INCLUDING ALL);" statement',
                             TG_TABLE_NAME);
      RETURN NULL;
    END;
    $function$;
    
    CREATE TABLE foo(a int, b int); -- doesn't hold data ever
    CREATE TRIGGER foo_disable_dml
       BEFORE INSERT OR UPDATE OR DELETE ON foo
       EXECUTE PROCEDURE disable_dml();

    postgres=# INSERT INTO  foo VALUES(10,20);
    ERROR:  this instance of foo table doesn't allow any DML operation
    HINT:  you should to run "CREATE TEMP TABLE foo(LIKE foo INCLUDING ALL);" statement
    postgres=# 
    
    CREATE TABLE
    postgres=# INSERT INTO  foo VALUES(10,20);
    INSERT 0 1

This trick emulates GLOBAL TEMP tables partially and it allows a statical validation.
Other possibility is using a [template foreign data wrapper] (https://github.com/okbob/template_fdw)

The usage of template_fdw is safer, because any regress tests fails immediately when any foreign
table based on tempate_fdw is used.

