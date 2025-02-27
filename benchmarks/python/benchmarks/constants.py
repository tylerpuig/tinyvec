import os
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BASE_PATH = os.path.join(SCRIPT_DIR, "db-files")
COLLECTION_NAME = "test_collection"
DIMENSIONS = 512
INSERTION_COUNT = 10_000
SLEEP_TIME = 3
QUERY_ITERATIONS = 10
TOP_K = 10
BASE_PATH = "./db-files/"
QDRANT_PATH = BASE_PATH + "qdrant-db/"
SQLITE_VEC_PATH = BASE_PATH + "sqlite/sqlite.db"
TINYVEC_PATH = BASE_PATH + "tinyvec/tinyvec.db"
LANCEDB_PATH = BASE_PATH + "lancedb"
CHROMA_PATH = BASE_PATH + "chroma-db"
