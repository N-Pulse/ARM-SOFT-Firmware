import pickle

file_path = "Testing\PyUART\rf_model.pkl"

with open(file_path, "rb") as f:
    data = pickle.load(f)

print(type(data))
print(data)