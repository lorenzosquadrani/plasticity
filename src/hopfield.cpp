#include <hopfield.h>

Hopfield :: Hopfield (const int & outputs, const int & batch_size, update_args optimizer,
                      float mu, float sigma, float delta, float p, int k, int seed
                      ) : BasePlasticity (outputs, batch_size, transfer :: _linear_, optimizer, mu, sigma, seed),
                          k (k), delta (delta), p (p)
{
  const int size = this->outputs * this->batch;
  this->fire_indices.reset(new int[size]);
  this->yl.reset(new float[size]);

  for (int i = 0; i < this->batch; ++i)
    for (int j = 0; j < this->outputs; ++j)
    {
      const int idx = i * this->outputs + j;
      this->fire_indices[idx] = j * this->batch + i;
    }

  this->check_params();
}


Hopfield :: Hopfield (const Hopfield & b) : BasePlasticity (b)
{
  const int size = b.outputs * b.batch;
  this->fire_indices.reset(new int[size]);

  this->yl.reset(new float[size]);

  for (int i = 0; i < this->batch; ++i)
    for (int j = 0; j < this->outputs; ++j)
    {
      const int idx = i * this->outputs + j;
      this->fire_indices[idx] = j * this->batch + i;
    }

  this->k = b.k;
  this->delta = b.delta;
  this->p = b.p;
}

Hopfield & Hopfield :: operator = (const Hopfield & b)
{
  BasePlasticity :: operator = (b);

  const int size = b.outputs * b.batch;
  this->fire_indices.reset(new int[size]);

  for (int i = 0; i < this->batch; ++i)
    for (int j = 0; j < this->outputs; ++j)
    {
      const int idx = i * this->outputs + j;
      this->fire_indices[idx] = j * this->batch + i;
    }

  this->k = b.k;
  this->delta = b.delta;
  this->p = b.p;

  return *this;
}


void Hopfield :: check_params ()
{
  if ( this->k < 2 )
  {
    std :: cerr << "k must be an integer bigger or equal than 2" << std :: endl;
    std :: exit(ERROR_K_POSITIVE);
  }
}

void Hopfield :: weights_update (float * X, const int & n_features, float * weights_update)
{

#ifdef _OPENMP
  #pragma omp for
#endif
  for (int i = 0; i < this->outputs * this->batch; ++i)
    this->yl[i] = 0.f;

#ifdef _OPENMP
  #pragma omp for
#endif
  for (int i = 0; i < this->nweights; ++i)
    weights_update[i] = 0.f;

#ifdef _OPENMP
  #pragma omp for
#endif
  for (int i = 0; i < this->batch; ++i)
  {
    const int idx = i * this->outputs;
    int * indices = this->fire_indices.get() + idx;
    std :: sort(indices, indices + this->outputs,
                [&](const int & i, const int & j)
                {
                  return this->output[i] < this->output[j];
                });
  }

#ifdef _OPENMP
  #pragma omp for
#endif
  for (int i = 0; i < this->batch; ++i)
  {
    const int idx = i * this->outputs + this->outputs;
    const int up_idx = this->fire_indices[idx - 1];
    const int rest_idx = this->fire_indices[idx - this->k];
    this->yl[up_idx] = 1.f;
    this->yl[rest_idx] = - this->delta;
  }

#ifdef _OPENMP
  #pragma omp for
#endif
  for (int i = 0; i < this->outputs; ++i)
  {
    const int idx = i * this->batch;
    this->theta[i] = std :: inner_product(this->yl.get() + idx,
                                          this->yl.get() + idx + this->batch,
                                          this->output.get() + idx,
                                          0.f);
  }

  // weights (outputs, n_features)
  // X (batch, n_features)
  // theta (outputs, )
  // yl (outputs, batch)
  // this->nweights = this->outputs * n_features;
  //

#ifdef _OPENMP
  #pragma omp for collapse (2)
#endif
  for (int i = 0; i < this->outputs; ++i)
    for (int j = 0; j < this->batch; ++j)
    {
      const int idx = i * this->batch + j;
      const float A_PART = this->yl[idx];

      float * xi = X + j * n_features;
      float * wi = weights_update + i * n_features;

      for (int k = 0; k < n_features; ++k)
        wi[k] += A_PART * xi[k];
    }

#ifdef _OPENMP
  #pragma omp for
#endif
  for (int i = 0; i < this->outputs; ++i)
  {
    const float theta_value = this->theta[i];
    for (int j = 0; j < n_features; ++j)
    {
      const int idx = i * n_features + j;
      weights_update[idx] -= theta_value * this->weights[idx];
    }
  }

  static float nc;

  nc = 0.f;

#ifdef _OPENMP
  #pragma omp for reduction (max : nc)
#endif
  for (int i = 0; i < this->nweights; ++i)
  {
    const float out = std :: fabs(weights_update[i]);
    nc = nc < out ? out : nc;
  }

#ifdef _OPENMP
  #pragma omp single
#endif
  nc = 1.f / std :: max(nc, BasePlasticity :: precision);


#ifdef _OPENMP
  #pragma omp for
#endif
  for (int i = 0; i < this->nweights; ++i)
    //weights_update[i] *= nc;
    weights_update[i] *= - nc; // Add the minus for compatibility with optimization algorithms

}


void Hopfield :: normalize_weights ()
{
  if ( this->p != 2.f )
  {

#ifdef _OPENMP

    #pragma omp for
    for (int i = 0; i < this->nweights; ++i)
    {
      const float wi = this->weights[i];
      this->weights[i] = std :: copysign( std :: pow(std :: fabs(wi), this->p - 1.f), wi );
    }

#else

    std :: transform(this->weights.get(), this->weights.get() + this->nweights,
                     this->weights.get(),
                     [&](const float & wi)
                     {
                       return std :: copysign( std :: pow(std :: fabs(wi), this->p - 1.f), wi );
                     });

#endif

  }
}
