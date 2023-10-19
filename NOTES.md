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

Where does line $L$ intersect a given plane $(\overrightarrow{N}, P) + D = 0$?


