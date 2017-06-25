#ifndef SQLITE_LOCKED_H
#define SQLITE_LOCKED_H

struct sqlite3;
struct sqlite3_stmt;

int sqlite3_blocking_step(sqlite3_stmt *pStmt);

int sqlite3_blocking_prepare_v2(
    sqlite3 *db,              /* Database handle. */
    const char *zSql,         /* UTF-8 encoded SQL statement. */
    int nSql,                 /* Length of zSql in bytes. */
    sqlite3_stmt **ppStmt,    /* OUT: A pointer to the prepared statement */
    const char **pz           /* OUT: End of parsed string */
);

#endif /* SQLITE_LOCKED_H */

