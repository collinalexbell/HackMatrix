from torchvision import datasets, transforms
from tinygrad.tensor import Tensor
from tinygrad.nn.state import get_parameters
from extra.datasets import fetch_mnist
from extra.training import train, evaluate
import sys

import numpy as np

from net import Net
from tinygrad.nn import optim

batch_size = 64

model = Net()
optimizer = optim.Adam(get_parameters(model), lr=0.0002, b1=0.5)

X_train, Y_train, X_test, Y_test = fetch_mnist()
X_train = X_train.reshape(-1, 28, 28).astype(np.uint8)
X_test = X_test.reshape(-1, 28, 28).astype(np.uint8)

steps = len(X_train)//batch_size
np.random.seed(1337) # loll33t

lmbd = 0.00025
lossfn = lambda out,y: out.sparse_categorical_crossentropy(y)

if(len(sys.argv) == 1):
    X_aug = X_train
    print(X_aug.shape)
    train(model, X_aug, Y_train, optimizer, steps=steps, lossfn=lossfn, BS=batch_size)
    accuracy = evaluate(model, X_test, Y_test, BS=batch_size)
    model.save("mnist")

if(len(sys.argv) == 2):
    modelName = sys.argv[1]
    model.load(modelName)

    input = list()
    fc1 = list()
    fc2 = list()
    fc3 = list()
    fc4 = list()
    output = list()

    result = model.forward(Tensor(X_test))
    print(result.numpy()[0])

    for i in range(100):
        x = Tensor(X_test[i])
        x = x.reshape([-1, 28*28])
        input.append(np.abs(x[0].detach().numpy()))
        x = model.fc1(x)
        x = x.leakyrelu()
        fc1.append(np.abs(x[0].detach().numpy()))
        x = model.fc2(x)
        x = x.leakyrelu()
        fc2.append(np.abs(x[0].detach().numpy()))
        x = model.fc3(x)
        x = x.leakyrelu()
        fc3.append(np.abs(x[0].detach().numpy()))
        x = model.fc4(x)
        x = x.leakyrelu()
        fc4.append(np.abs(x[0].detach().numpy()))
        x = x.log_softmax()
        output.append(np.abs(x[0].detach().numpy()))
    input = np.array(input)
    fc1 = np.array(fc1)
    fc2 = np.array(fc2)
    fc3 = np.array(fc3)
    fc4 = np.array(fc4)
    output = np.array(output)
    np.savez('activity.npz', input=input, fc1=fc1, fc2=fc2, fc3=fc3, fc4=fc4, output=output,)
