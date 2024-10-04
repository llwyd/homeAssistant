import matplotlib.pyplot as plt
from matplotlib.figure import Figure
import numpy as np
import sqlalchemy as db
import pandas as pd

engine = db.create_engine('sqlite:///app.db')
conn = engine.connect()
metadata = db.MetaData()

environment_data = db.Table('environment_data', metadata, autoload_with=engine)

query = environment_data.select().where(environment_data.columns.device_id == "bedroom")

query_output = conn.execute(query)

df = pd.DataFrame(query_output.fetchall())
plt.plot(np.float32(df['temperature']))
plt.show()

