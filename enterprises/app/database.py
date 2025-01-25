from sqlalchemy.ext.asyncio import create_async_engine, AsyncSession, async_sessionmaker
from sqlalchemy.orm import declarative_base
from app.config import get_config

config = get_config()

# Determine the correct driver and engine arguments based on the database URL
if config.database_url.startswith('postgresql'):
    driver = 'postgresql+asyncpg'
    engine_args = {
        "echo": True,
        "pool_pre_ping": True,
        "pool_size": 5,
        "max_overflow": 10,
    }
elif config.database_url.startswith('mysql'):
    driver = 'mysql+aiomysql'
    engine_args = {
        "echo": True,
        "pool_pre_ping": True,
        "pool_size": 5,
        "max_overflow": 10,
    }
elif config.database_url.startswith('sqlite'):
    driver = 'sqlite+aiosqlite'
    engine_args = {
        "connect_args": {"check_same_thread": False},
    }
else:
    raise ValueError("Unsupported database type")

# Create the async engine
engine = create_async_engine(
    config.database_url.replace(config.database_url.split('://')[0], driver),echo=True,
    **engine_args
)

# Create async session factory
AsyncSessionLocal = async_sessionmaker(
    engine, 
    class_=AsyncSession, 
    expire_on_commit=False,
    autocommit=False,
    autoflush=False,
)

Base = declarative_base()

async def init_db():
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)

# Dependency
async def get_async_session() -> AsyncSession:
    async with AsyncSessionLocal() as session:
        yield session