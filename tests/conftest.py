import json
import pytest

from testsuite.databases.pgsql import discover
from testsuite.databases.redis import service as redis_service

pytest_plugins = [
    'pytest_userver.plugins.core',
    'pytest_userver.plugins.postgresql',
    'pytest_userver.plugins.redis',
]

USERVER_CONFIG_HOOKS = ['userver_config_disable_stack_usage_monitor']


@pytest.fixture(scope='session')
def userver_config_disable_stack_usage_monitor():
    """Disable StackUsageMonitor (requires userfaultfd, not available in Docker)."""
    def patch_config(config_yaml, config_vars):
        coro_pool = config_yaml['components_manager'].setdefault('coro_pool', {})
        coro_pool['stack_usage_monitor_enabled'] = False
    return patch_config


@pytest.fixture(scope='session')
def initial_data_path(service_source_dir):
    """Path for find files with data"""
    return [
        service_source_dir / 'postgresql/data',
    ]


@pytest.fixture(scope='session')
def pgsql_local(service_source_dir, pgsql_local_create):
    """Create schemas databases for tests"""
    databases = discover.find_schemas(
        'social_net_service',
        [service_source_dir.joinpath('postgresql/schemas')],
    )
    return pgsql_local_create(list(databases.values()))


@pytest.fixture(scope='session')
def userver_pg_config(pgsql_local):
    """
    Override to handle multiple Postgres components (main DB + shards).
    Maps each component name to the corresponding pgsql_local database.
    """
    component_db_map = {
        'postgres-db-1': 'db_1',
        'postgres-citus': 'citus_dialogs_test',
    }

    def _patch_config(config_yaml, config_vars):
        components = config_yaml['components_manager']['components']
        for comp_name, db_key in component_db_map.items():
            if comp_name in components and db_key in pgsql_local:
                components[comp_name]['dbconnection'] = pgsql_local[db_key].get_uri()
                components[comp_name].pop('dbalias', None)

    return _patch_config


@pytest.fixture(scope='session')
def _redis_service_settings():
    """Force Redis to bind on 127.0.0.1 to avoid host-resolution issues in Docker."""
    settings = redis_service.get_service_settings()
    return redis_service.ServiceSettings(
        host='127.0.0.1',
        master_ports=settings.master_ports,
        sentinel_port=settings.sentinel_port,
        slave_ports=settings.slave_ports,
    )


@pytest.fixture(scope='session')
def service_env(redis_sentinels):
    """Configure Redis connection via secdist"""
    secdist_config = {
        'redis_settings': {
            'feed-redis': {
                'password': '',
                'sentinels': redis_sentinels,
                'shards': [{'name': 'test_master0'}],
            },
        },
    }
    return {'SECDIST_CONFIG': json.dumps(secdist_config)}
