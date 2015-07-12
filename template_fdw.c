/*-------------------------------------------------------------------------
 *
 * template_fdw.c
 *
 *			  a template foreign data wrapper
 *
 * by Pavel Stehule 2013, 2014, 2015
 *
 *-------------------------------------------------------------------------
 *
 * Notes:
 *
 *   This wrapper doesn't allow any DML or SELECT operation.
 *
 */
#include "postgres.h"

#include "funcapi.h"
#include "miscadmin.h"


#include "access/reloptions.h"
#include "foreign/fdwapi.h"
#include "foreign/foreign.h"
#include "optimizer/pathnode.h"
#include "optimizer/planmain.h"
#include "optimizer/restrictinfo.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"

#include "template_fdw_builtins.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

/* callback functions */
static void templateGetForeignRelSize(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);
static void templateGetForeignPaths(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);
static ForeignScan *templateGetForeignPlan(PlannerInfo *root, RelOptInfo *baserel,
								 Oid foreigntableid, ForeignPath *best_path,
								 List *tlist, List *scan_clauses);
static void templateBeginForeignScan(ForeignScanState *node, int eflags);
static TupleTableSlot *templateIterateForeignScan(ForeignScanState *node);
static void templateReScanForeignScan(ForeignScanState *node);
static void templateEndForeignScan(ForeignScanState *node);
static void templateAddForeignUpdateTargets(Query *parsetree, RangeTblEntry *target_rte, Relation target_relation);
static List *templatePlanForeignModify(PlannerInfo *root, ModifyTable *plan, Index resultRelation, int subplan_index);
static void templateBeginForeignModify(ModifyTableState *mtstate, ResultRelInfo *rinfo,
										    List *fdw_private,
										    int subplan_index, int eflags);
static TupleTableSlot *templateExecForeignInsert(EState *estate,
								   ResultRelInfo *rinfo, TupleTableSlot *slot,
								   TupleTableSlot *planSlot);
static TupleTableSlot *templateExecForeignUpdate(EState *estate,
								   ResultRelInfo *rinfo, TupleTableSlot *slot,
								   TupleTableSlot *planSlot);
static TupleTableSlot *templateExecForeignDelete(EState *estate,
								   ResultRelInfo *rinfo,
								   TupleTableSlot *slot, TupleTableSlot *planSlot);
static void templateEndForeignModify(EState *estate, ResultRelInfo *rinfo);
static bool templateAnalyzeForeignTable(Relation relation, AcquireSampleRowsFunc *func, BlockNumber *totalpages);

PG_FUNCTION_INFO_V1(template_fdw_handler);
PG_FUNCTION_INFO_V1(template_fdw_validator);

Datum
template_fdw_handler(PG_FUNCTION_ARGS)
{
	FdwRoutine *fdwroutine = makeNode(FdwRoutine);

	/* assign the handlers for the FDW */

	/* these are required */
	fdwroutine->GetForeignRelSize = templateGetForeignRelSize;
	fdwroutine->GetForeignPaths = templateGetForeignPaths;
	fdwroutine->GetForeignPlan = templateGetForeignPlan;
	fdwroutine->BeginForeignScan = templateBeginForeignScan;
	fdwroutine->IterateForeignScan = templateIterateForeignScan;
	fdwroutine->ReScanForeignScan = templateReScanForeignScan;
	fdwroutine->EndForeignScan = templateEndForeignScan;

	/* remainder are optional - use NULL if not required */
	/* support for insert / update / delete */
	fdwroutine->AddForeignUpdateTargets = templateAddForeignUpdateTargets;
	fdwroutine->PlanForeignModify = templatePlanForeignModify;
	fdwroutine->BeginForeignModify = templateBeginForeignModify;
	fdwroutine->ExecForeignInsert = templateExecForeignInsert;
	fdwroutine->ExecForeignUpdate = templateExecForeignUpdate;
	fdwroutine->ExecForeignDelete = templateExecForeignDelete;
	fdwroutine->EndForeignModify = templateEndForeignModify;

	/* support for EXPLAIN */
	fdwroutine->ExplainForeignScan = NULL;
	fdwroutine->ExplainForeignModify = NULL;

	/* support for ANALYSE */
	fdwroutine->AnalyzeForeignTable = templateAnalyzeForeignTable;

	PG_RETURN_POINTER(fdwroutine);
}

Datum
template_fdw_validator(PG_FUNCTION_ARGS)
{
	List	   *options_list = untransformRelOptions(PG_GETARG_DATUM(0));

	if (list_length(options_list) > 0)
		ereport(ERROR,
			(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
			 errmsg("invalid options"),
			 errhint("template FDW doies not support any options")));

	PG_RETURN_VOID();
}

static void
templateGetForeignRelSize(PlannerInfo *root,
			   RelOptInfo *baserel,
			   Oid foreigntableid)
{
	baserel->fdw_private = NULL;
	baserel->rows = 0;
}

static void
templateGetForeignPaths(PlannerInfo *root,
			 RelOptInfo *baserel,
			 Oid foreigntableid)
{
	add_path(baserel, (Path *)
		 create_foreignscan_path(root, baserel,
							     0, 0, 0,
									 NIL, NULL, NIL));
}

static ForeignScan *
templateGetForeignPlan(PlannerInfo *root,
			RelOptInfo *baserel,
			Oid foreigntableid,
			ForeignPath *best_path,
			List *tlist,
			List *scan_clauses)
{
	Index		scan_relid = 1;

	scan_clauses = extract_actual_clauses(scan_clauses, false);

	/* Create the ForeignScan node */
#if PG_VERSION_NUM >= 90500

	return make_foreignscan(tlist,
					    scan_clauses,  scan_relid,
								        NIL, NIL, NIL);

#else

	return make_foreignscan(tlist,
					    scan_clauses,  scan_relid,
								        NIL, NIL);

#endif

}

static void
templateBeginForeignScan(ForeignScanState *node,
			  int eflags)
{
}


static TupleTableSlot *
templateIterateForeignScan(ForeignScanState *node)
{
	Relation rel = node->ss.ss_currentRelation;
	char *template_name,
			*table_name;

	template_name = quote_qualified_identifier(get_namespace_name(RelationGetNamespace(rel)),
										     RelationGetRelationName(rel));
	table_name = quote_qualified_identifier(NULL, RelationGetRelationName(rel));

	ereport(ERROR,
			(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
			 errmsg("cannot read from table \"%s\"", template_name),
			 errdetail("Table is template."),
			 errhint("Create temp table by statement "
			 "\"CREATE TEMP TABLE %s(LIKE %s INCLUDING ALL);\"", table_name, template_name)));
}


static void
templateReScanForeignScan(ForeignScanState *node)
{
}


static void
templateEndForeignScan(ForeignScanState *node)
{
}


static void
templateAddForeignUpdateTargets(Query *parsetree,
				 RangeTblEntry *target_rte,
				 Relation target_relation)
{
}


static List *
templatePlanForeignModify(PlannerInfo *root,
			   ModifyTable *plan,
			   Index resultRelation,
			   int subplan_index)
{
    return NULL;
}


static void
templateBeginForeignModify(ModifyTableState *mtstate,
			    ResultRelInfo *rinfo,
			    List *fdw_private,
			    int subplan_index,
			    int eflags)
{
}


static TupleTableSlot *
templateExecForeignInsert(EState *estate,
			   ResultRelInfo *rinfo,
			   TupleTableSlot *slot,
			   TupleTableSlot *planSlot)
{

	Relation rel = rinfo->ri_RelationDesc;
	char *template_name,
			*table_name;

	template_name = quote_qualified_identifier(get_namespace_name(RelationGetNamespace(rel)),
										     RelationGetRelationName(rel));
	table_name = quote_qualified_identifier(NULL, RelationGetRelationName(rel));

	ereport(ERROR,
			(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
			 errmsg("cannot insert into to table \"%s\"", template_name),
			 errdetail("Table is template."),
			 errhint("Create temp table by statement "
			 "\"CREATE TEMP TABLE %s(LIKE %s INCLUDING ALL);\"", table_name, template_name)));
}


static TupleTableSlot *
templateExecForeignUpdate(EState *estate,
			   ResultRelInfo *rinfo,
			   TupleTableSlot *slot,
			   TupleTableSlot *planSlot)
{
	Relation rel = rinfo->ri_RelationDesc;
	char *template_name,
			*table_name;

	template_name = quote_qualified_identifier(get_namespace_name(RelationGetNamespace(rel)),
										     RelationGetRelationName(rel));
	table_name = quote_qualified_identifier(NULL, RelationGetRelationName(rel));

	ereport(ERROR,
			(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
			 errmsg("cannot update table \"%s\"", template_name),
			 errdetail("Table is template."),
			 errhint("Create temp table by statement "
			 "\"CREATE TEMP TABLE %s(LIKE %s INCLUDING ALL);\"", table_name, template_name)));
}


static TupleTableSlot *
templateExecForeignDelete(EState *estate,
			   ResultRelInfo *rinfo,
			   TupleTableSlot *slot,
			   TupleTableSlot *planSlot)
{
	Relation rel = rinfo->ri_RelationDesc;
	char *template_name,
			*table_name;

	template_name = quote_qualified_identifier(get_namespace_name(RelationGetNamespace(rel)),
										     RelationGetRelationName(rel));
	table_name = quote_qualified_identifier(NULL, RelationGetRelationName(rel));

	ereport(ERROR,
			(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
			 errmsg("cannot delete from table \"%s\"", template_name),
			 errdetail("Table is template."),
			 errhint("Create temp table by statement "
			 "\"CREATE TEMP TABLE %s(LIKE %s INCLUDING ALL);\"", table_name, template_name)));
}


static void
templateEndForeignModify(EState *estate,
			  ResultRelInfo *rinfo)
{
}

static bool
templateAnalyzeForeignTable(Relation relation,
			     AcquireSampleRowsFunc *func,
			     BlockNumber *totalpages)
{
    return false;
}
