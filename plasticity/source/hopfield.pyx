# distutils: language = c++
# cython: language_level=2

from cython.operator cimport dereference as deref
from libcpp.string cimport string

from hopfield cimport Hopfield


cdef class _Hopfield:

  def __init__ (self, int outputs, int batch_size, int activation, float mu, float sigma, float epsilon, int seed, float delta, float p, int k):

    self.thisptr.reset(new Hopfield(outputs, batch_size, mu, sigma, epsilon, delta, p, k, seed))
    self.outputs = outputs
    self.n_features = 0

  def fit (self, float[::1] X, int n_samples, int n_features, int num_epochs):

    self.n_features = n_features
    deref(self.thisptr).fit(&X[0], n_samples, n_features, num_epochs)

  def predict (self, float[::1] X, int n_samples, int n_features):

    cdef float * res = deref(self.thisptr).predict(&X[0], n_samples, n_features)
    return [res[i] for i in range(self.outputs * n_samples)]

  def get_weights (self):

    if self.n_features == 0:
      return (None, None)

    cdef float * w = deref(self.thisptr).get_weights()
    return ([w[i] for i in range(self.outputs * self.n_features)], (self.outputs, self.n_features))

  def save_weights (self, string filename):
    deref(self.thisptr).save_weights(filename)

  def load_weights (self, string filename):
    deref(self.thisptr).load_weights(filename)