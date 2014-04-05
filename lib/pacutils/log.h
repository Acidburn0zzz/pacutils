#ifndef PACUTILS_LOG_H
#define PACUTILS_LOG_H

typedef enum {
	PU_LOG_OPERATION_INSTALL,
	PU_LOG_OPERATION_REINSTALL,
	PU_LOG_OPERATION_UPGRADE,
	PU_LOG_OPERATION_DOWNGRADE,
	PU_LOG_OPERATION_REMOVE,
} pu_log_operation_t;

typedef struct {
	pu_log_operation_t operation;
	char *target;
	char *old_version;
	char *new_version;
} pu_log_action_t;

typedef struct {
	struct tm *timestamp;
	char *caller;
	char *message;
} pu_log_entry_t;

int pu_log_fprint_entry(FILE *stream, pu_log_entry_t *entry);
alpm_list_t *pu_log_parse_file(FILE *stream);
void pu_log_entry_free(pu_log_entry_t *entry);

pu_log_action_t *pu_log_action_parse(char *message);
void pu_log_action_free(pu_log_action_t *action);

#endif

/* vim: set ts=2 sw=2 noet: */