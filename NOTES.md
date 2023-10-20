# TODO

- [x] winding order (clockwise)
- [x] backface culling w/normal
- [x] fill triangle
- [x] shading + wireframe
- [x] obj loader 
- [ ] Line + Triangle clipping against near plane + screen bounds
- [ ] Texture mapping
- [x] Clip-space line clipping (Liang-Barsky or Cohen-Sutherland)


## Clipping

Curious, does GitHub markdown rendering respect MathJax?


### General Equation of a Plane

$$Ax + By + Cz + D = 0$$
$ABC$ defines the plane's normal, $\overrightarrow{N}$, so if we imagine the z-clipping plane in 3D graphics, you get

$$ 0x + 0y + 1z + D = 0 $$

Where $-D$ is the **signed distance** of the plane to the origin. So the signed distance to a plane is simply the inner product of the plane's normal and the point in question. Trivially, point $0,1,2$ for a z-plane at $0.1$ has a signed distance of

$$ (0\cdot0) + (0\cdot1) + (1\cdot2) - 0.1 = 1.9 $$

And we can prove that a point on the plane has a signed distance of zero, e.g., $(2,2,.1)$

$$ (0\cdot2) + (0\cdot2) + (1\cdot.1) + -0.1 = 0 $$

## Intersection with a plane

Where does line $AB$ intersect a given plane $\langle\overrightarrow{N}, P\rangle + D = 0$?

Compute scalar $t$ that satisfies the standard plane equation:

$$ \langle N, A + t(B - A) \rangle + D = 0; 0 \le t \le 1 $$

$$ \langle N, A \rangle + t \langle N,B - A\rangle + D = 0 $$
$$ t = \frac{-D + \langle N, A \rangle}{\langle N,B-A \rangle} $$
Then the intersection $Q$ is simply
$$ Q = A + t(B - A) $$
For example, for line $(2,2,2), (2,2,-2)$ we intuitively expect it to intersect at $(2,2,.1)$ for our hypothetical z-plane.

$$ t = \frac{-.1 + (0\cdot2+0\cdot2+1\cdot2)}{0\cdot0+0\cdot0+-4} $$
$$ t = \frac{1.9}{-4} $$
$$ t = -0.475 $$
$$ Q = (2,2,2) + -0.475(0,0,-4) = (2,2,.1) $$

Works!
