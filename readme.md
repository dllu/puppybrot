High resolution Buddhabrot generator
====================================

Generates high resolution pictures of the [Buddhabrot](https://en.wikipedia.org/wiki/Buddhabrot) using standard modern C++ with very few dependencies.

![pic](https://pics.dllu.net/file/dllu-pics/cubehelix_buddhabrot_512_16384_2000_1024.png)

16384 x 16384 version: [**large 95 MiB file**](https://pics.dllu.net/file/dllu-pics/cubehelix_buddhabrot_16384_2000_1024.png)

### Requirements

* [`png++`](https://www.nongnu.org/pngpp/) (e.g. on Ubuntu, `sudo apt install libpng++-dev libpng-dev`)

# Building

On UNIX-like systems,

```
g++ buddhabrot.cpp -Ofast -march=native -lpng -lpthread -o buddhabrot
```

or

```
clang++ buddhabrot.cpp -Ofast -march=native -lpng -lpthread -o buddhabrot
```

## Optional: CubeHelix colouring

```
g++ cubehelix.cpp -Ofast -march=native -lpng -o cubehelix
```

or

```
clang++ buddhabrot.cpp -O3 -march=native -lpng -lpthread -o buddhabrot
```

# Running

```
./buddhabrot image_size iterations num_threads max_samples_per_pixel
```

* `image_size` is how big your image is. The output will always be a square iamge representing the complex plane from -2 to 2 on each axis.
* `iterations` is the max iterations.
* `num_threads` is the number of threads to use. Use the physical cores, not logical threads. For example my AMD Ryzen 9 3900X performs better with `num_threads = 12` although it is hyperthreaded and has 24 logical threads. Please also note that memory usage scales with number of threads, so if you are running out of RAM, you may wish to use fewer threads.
* `max_samples_per_pixel` is the maximum number of random samples per pixel. If this value is too low, the output may be grainy.

The program will automatically output a 16-bit grayscale PNG image that is brightness-normalized and gamma-corrected.
If it appears washed out, you can adjust the constrast in your preferred image editing program.
The 16-bit depth is much more than conventional 8-bit images so you have lots of leeway to adjust the image.

The program `cubehelix` will apply the [CubeHelix colour palette](http://www.mrao.cam.ac.uk/~dag/CUBEHELIX/) to output a cool-looking colourful image, with an option to adjust the contrast as needed.

# Theory

The [Buddhabrot](https://en.wikipedia.org/wiki/Buddhabrot) is the probability distribution over trajectories that escape the Mandelbrot fractal.

The pseudocode is the following:

```
function buddhabrot(size, max_iterations):
    image = zeros(size, size)
    for some random pixels c:
        z = complex(0, 0)
        trajectory = []
        for iterations in range(max_iterations):
            z = z * z + c
            trajectory.append(z)
            if |z| > escape_radius:
                for point in trajectory:
                    image[point.x, point.y]++
                break
    return image
```

However, naively choosing the random pixels will be very slow.

We notice that the vast majority of random pixels `c` don't do much because they either escape very quickly, or they never escape.

Hence, we use a crude version of _importance sampling_.

For a given region, we can estimate how likely it to be interesting by starting out with very few random samples. If these early samples all turn out to be boring (e.g. if all of them fail to escape), then we can terminate the sampling immediately.

However, if they do escape, we can look at how fast they escape. If they escape really quickly, then it is kind of boring too, so we can terminate after a few samples. But if some of them take a long time to escape, then these are interesting trajectories, so we will increase the number of samples accordingly.

Another note is that the most interesting samples tend to happen on the edges of the Mandelbrot set, so if we find that, for a given region, some points escape and some don't, then this is a good indicator that it is on the edge and therefore supremely interesting indeed.

Finally, after sampling, the pixel values along the trajectory should be divided by the number of samples.

Although my code is not well optimized, on my computer it is able to generate a 1000 iteration 16384 x 16384 image with up to 128 samples per pixel within 4 minutes.

```
g++ buddhabrot.cpp -Ofast -march=native -lpng -lpthread -o buddhabrot; and time ./buddhabrot 16384 1000 12 128

________________________________________________________
Executed in  225.85 secs   fish           external
   usr time   34.89 mins    0.00 micros   34.89 mins
   sys time    0.09 mins  418.00 micros    0.09 mins
```

## Related links

* [Benedikt Bitterli's excellent GPU implementation](https://benedikt-bitterli.me/buddhabrot/) also uses importance sampling. He does so in two passes, the first pass to estimate the importance, and then the second pass to sample accordingly. In constrast, my algorithm adjusts the number of samples as needed as it goes. Benedikt's algorithm is more suitable for GPU implementation as it likely avoids a lot of branching.
* [Alexander Boswell](http://www.steckles.com/buddha/) uses the Metropolis-Hastings algorithm. Instead of making independent samples, it essentially does a little random walk to find the interesting regions, assuming that there are likely interesting regions next to existing interesting regions. This also works quite well as you can avoid the first few samples in obviously uninteresting parts. However, the interesting part about the Buddhabrot is on the edge of the Mandelbrot set, which is not very contiguous, so it is hard for this random walk to explore it properly.

