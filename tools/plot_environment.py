import matplotlib.pyplot as plt
from matplotlib.figure import Figure
import numpy as np
import sqlalchemy as db
import pandas as pd

engine = db.create_engine('sqlite:///app.db')
conn = engine.connect()
metadata = db.MetaData()

environment_data = db.Table('environment_data', metadata, autoload_with=engine)

#query = environment_data.select().where(environment_data.columns.device_id == "bedroom")

query = environment_data.select()
query_output = conn.execute(query)

df = pd.DataFrame(query_output.fetchall())

df['temperature'] = df['temperature'].astype(float)
df['humidity'] = df['humidity'].astype(float)

nodes = df.device_id.unique()

for node in nodes:
    data = df[df['device_id'] == node]
    plt.plot(np.float32(data['temperature']))

plt.legend(nodes)
plt.show()

