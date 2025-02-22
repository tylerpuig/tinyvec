export function generateRandomVector(dimensions: number) {
  const vector = new Float32Array(dimensions);
  for (let i = 0; i < dimensions; i++) {
    vector[i] = Math.random();
  }
  return vector;
}
