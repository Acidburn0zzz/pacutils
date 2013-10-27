#include <getopt.h>

#include <pacutils.h>

#define LOG_PREFIX "PACINSTALL"

pu_config_t *config = NULL;
alpm_handle_t *handle = NULL;
alpm_loglevel_t log_level = ALPM_LOG_ERROR | ALPM_LOG_WARNING;
alpm_transflag_t trans_flags = 0;

enum longopt_flags {
	FLAG_ASDEPS,
	FLAG_ASEXPLICIT,
	FLAG_CACHEDIR,
	FLAG_CONFIG,
	FLAG_DBPATH,
	FLAG_DEBUG,
	FLAG_DLONLY,
	FLAG_HELP,
	FLAG_LOGFILE,
	FLAG_ROOT,
	FLAG_VERSION,
};

void fatal(const char *msg)
{
	fputs(msg, stderr);
	exit(1);
}

void usage(int ret)
{
	FILE *stream = (ret ? stderr : stdout);
	fputs("pacinstall - install packages from sync repositories\n", stream);
	fputs("usage:  pacinstall [options] <package>...\n", stream);
	fputs("        pacinstall (--help|--version)\n", stream);
	fputs("options:\n", stream);
	fputs("   --asdeps           install packages as dependencies\n", stream);
	fputs("   --asexplicit       install packages as explicit\n", stream);
	fputs("   --cachedir=<path>  set an alternate cache location\n", stream);
	fputs("   --config=<path>    set an alternate configuration file\n", stream);
	fputs("   --dbpath=<path>    set an alternate database location\n", stream);
	fputs("   --debug            enable extra debugging messages\n", stream);
	fputs("   --downloadonly     download packages without installing\n", stream);
	fputs("   --logfile=<path>   set an alternate log file\n", stream);
	fputs("   --root=<path>      set an alternate installation root\n", stream);
	fputs("   --help             display this help information\n", stream);
	fputs("   --version          display version information\n", stream);
	exit(ret);
}

pu_config_t *parse_opts(int argc, char **argv)
{
	char *config_file = "/etc/pacman.conf";
	pu_config_t *config = NULL;
	int c;

	char *short_opts = "";
	struct option long_opts[] = {
		{ "asdeps"       , no_argument       , NULL , FLAG_ASDEPS       } ,
		{ "asexplicit"   , no_argument       , NULL , FLAG_ASEXPLICIT   } ,
		{ "config"       , required_argument , NULL , FLAG_CONFIG       } ,
		{ "dbpath"       , required_argument , NULL , FLAG_DBPATH       } ,
		{ "debug"        , no_argument       , NULL , FLAG_DEBUG        } ,
		{ "downloadonly" , no_argument       , NULL , FLAG_DLONLY       } ,
		{ "help"         , no_argument       , NULL , FLAG_HELP         } ,
		{ "root"         , required_argument , NULL , FLAG_ROOT         } ,
		{ "version"      , no_argument       , NULL , FLAG_VERSION      } ,
		{ "logfile"      , required_argument , NULL , FLAG_LOGFILE      } ,
		{ "cachedir"     , required_argument , NULL , FLAG_CACHEDIR     } ,
		{ 0, 0, 0, 0 },
	};

	/* check for a custom config file location */
	opterr = 0;
	c = getopt_long(argc, argv, short_opts, long_opts, NULL);
	while(c != -1) {
		if(c == FLAG_CONFIG) {
			config_file = optarg;
			break;
		}
		c = getopt_long(argc, argv, short_opts, long_opts, NULL);
	}

	/* load the config file */
	config = pu_config_new_from_file(config_file);
	if(!config) {
		fprintf(stderr, "error: could not parse '%s'\n", config_file);
		return NULL;
	}

	/* process remaining command-line options */
	optind = opterr = 1;
	c = getopt_long(argc, argv, short_opts, long_opts, NULL);
	while(c != -1) {
		switch(c) {
			case FLAG_ASDEPS:
				if(trans_flags & ALPM_TRANS_FLAG_ALLEXPLICIT) {
					fatal("error: --asdeps and --asexplicit may not be used together\n");
				}
				trans_flags |= ALPM_TRANS_FLAG_ALLDEPS;
				break;
			case FLAG_ASEXPLICIT:
				if(trans_flags & ALPM_TRANS_FLAG_ALLDEPS) {
					fatal("error: --asdeps and --asexplicit may not be used together\n");
				}
				trans_flags |= ALPM_TRANS_FLAG_ALLEXPLICIT;
				break;
			case FLAG_CACHEDIR:
				FREELIST(config->cachedirs);
				config->cachedirs = alpm_list_add(NULL, strdup(optarg));
				break;
			case FLAG_CONFIG:
				/* already handled */
				break;
			case FLAG_DBPATH:
				free(config->dbpath);
				config->dbpath = strdup(optarg);
				break;
			case FLAG_DEBUG:
				log_level |= ALPM_LOG_DEBUG;
				log_level |= ALPM_LOG_FUNCTION;
				break;
			case FLAG_DLONLY:
				trans_flags |= ALPM_TRANS_FLAG_DOWNLOADONLY;
				trans_flags |= ALPM_TRANS_FLAG_NOCONFLICTS;
				break;
			case FLAG_HELP:
				usage(0);
				break;
			case FLAG_LOGFILE:
				free(config->logfile);
				config->logfile = strdup(optarg);
				break;
			case FLAG_ROOT:
				free(config->rootdir);
				config->rootdir = strdup(optarg);
				break;
			case FLAG_VERSION:
				pu_print_version("pacinstall", "0.1");
				exit(0);
				break;
			case '?':
			default:
				usage(1);
				break;
		}
		c = getopt_long(argc, argv, short_opts, long_opts, NULL);
	}

	return config;
}

void cb_event(alpm_event_t event, void *data1, void *data2)
{
	switch(event) {
		case ALPM_EVENT_ADD_DONE:
			alpm_logaction(handle, LOG_PREFIX, "installed %s (%s)\n",
					alpm_pkg_get_name(data1), alpm_pkg_get_version(data1));
			break;
		case ALPM_EVENT_REMOVE_DONE:
			alpm_logaction(handle, LOG_PREFIX, "removed %s (%s)\n",
					alpm_pkg_get_name(data1), alpm_pkg_get_version(data1));
			break;
		case ALPM_EVENT_UPGRADE_DONE:
			alpm_logaction(handle, LOG_PREFIX, "upgraded %s (%s -> %s)\n",
					alpm_pkg_get_name(data1),
					alpm_pkg_get_version(data2), alpm_pkg_get_version(data1));
			break;
		case ALPM_EVENT_DOWNGRADE_DONE:
			alpm_logaction(handle, LOG_PREFIX, "downgraded %s (%s -> %s)\n",
					alpm_pkg_get_name(data1),
					alpm_pkg_get_version(data2), alpm_pkg_get_version(data1));
			break;
		case ALPM_EVENT_REINSTALL_DONE:
			alpm_logaction(handle, LOG_PREFIX, "reinstalled %s (%s)\n",
					alpm_pkg_get_name(data1), alpm_pkg_get_version(data1));
			break;
	}
}

void cb_log(alpm_loglevel_t level, const char *fmt, va_list args)
{
	if(level & log_level) {
		vprintf(fmt, args);
	}
}

int main(int argc, char **argv)
{
	alpm_list_t *i, *sync_dbs = NULL, *targets = NULL, *err_data = NULL;
	int ret = 0;

	if(!(config = parse_opts(argc, argv))) {
		goto cleanup;
	}

	if(!(handle = pu_initialize_handle_from_config(config))) {
		fprintf(stderr, "error: failed to initialize alpm.\n");
		ret = 1;
		goto cleanup;
	}

	alpm_option_set_eventcb(handle, cb_event);
	alpm_option_set_progresscb(handle, pu_cb_progress);
	alpm_option_set_dlcb(handle, pu_cb_download);
	alpm_option_set_logcb(handle, cb_log);

	sync_dbs = pu_register_syncdbs(handle, config->repos);
	if(!sync_dbs) {
		fprintf(stderr, "error: no valid sync dbs configured.\n");
		ret = 1;
		goto cleanup;
	}

	for(; optind < argc; optind++) {
		char *pkgname = argv[optind];
		char *dbname = NULL;
		char *sep = strchr(pkgname, '/');
		int found = 0;
		alpm_list_t *d;

		if(sep) {
			dbname = argv[optind];
			pkgname = sep + 1;
			*sep = '\0';
		}

		for(d = sync_dbs; d; d = d->next) {
			if(dbname && strcmp(alpm_db_get_name(d->data), dbname) != 0) {
				continue;
			}

			alpm_pkg_t *pkg = alpm_db_get_pkg(d->data, pkgname);
			if(pkg) {
				found = 1;
				targets = alpm_list_add(targets, pkg);
				break;
			}
		}

		if(!found) {
			if(dbname) {
				fprintf(stderr, "error: could not locate package '%s' in sync db '%s'\n", pkgname, dbname);
			} else {
				fprintf(stderr, "error: could not locate package '%s'\n", pkgname);
			}
			ret = 1;
		}
	}

	if(ret != 0) {
		goto cleanup;
	}

	/* notify the user of the targets we found */
	for(i = targets; i; i = i->next) {
		alpm_pkg_t *pkg = i->data;
		printf("installing '%s/%s'\n",
					alpm_db_get_name(alpm_pkg_get_db(pkg)), alpm_pkg_get_name(pkg));
	}

	if(!targets) {
		fprintf(stderr, "error: no targets provided.\n");
		ret = 1;
		goto cleanup;
	}

	if(alpm_trans_init(handle, trans_flags) != 0) {
		fprintf(stderr, "%s\n", alpm_strerror(alpm_errno(handle)));
		ret = 1;
		goto cleanup;
	}

	for(i = targets; i; i = i->next) {
		alpm_add_pkg(handle, i->data);
	}

	if(alpm_trans_prepare(handle, &err_data) != 0) {
		fprintf(stderr, "%s\n", alpm_strerror(alpm_errno(handle)));
		ret = 1;
		goto transcleanup;
	}

	if(alpm_trans_commit(handle, &err_data) != 0) {
		fprintf(stderr, "%s\n", alpm_strerror(alpm_errno(handle)));
		ret = 1;
		goto transcleanup;
	}

transcleanup:
	if(alpm_trans_release(handle) != 0) {
		fprintf(stderr, "%s\n", alpm_strerror(alpm_errno(handle)));
		ret = 1;
	}

cleanup:
	alpm_list_free(sync_dbs);
	alpm_list_free(err_data);
	alpm_list_free(targets);
	alpm_release(handle);
	pu_config_free(config);

	return ret;
}

/* vim: set ts=2 sw=2 noet: */