#pragma once
static const char* camMathSample(unsigned page) {
  static const char* samples[]={
    R"(上行中文必须完整
\boxed{x\in(-\infty,-1]}
x^{n+1}+a_{i+1}=0
Fe^{3+}、SO_4^{2-}
下行中文不得被遮挡)",
    R"(分数与根式
\frac{x+1}{x-1}=2
\sqrt{x^2+1}\ge 1
\sum_{i=1}^{n}i=\frac{n(n+1)}{2}
\int_0^1 x^2 dx=\frac{1}{3})",
    R"(化学反应与电荷
\ce{2H2 + O2 -> 2H2O}
\ce{Ca(OH)2 + 2HCl -> CaCl2 + 2H2O}
\ce{Fe^{3+} + 3OH^- -> Fe(OH)3}
\ce{N2 + 3H2 <=> 2NH3}
上下行中文均需完整)",
    R"(# 函数与化学
**粗体需要实际加粗**
| 量 | 表达式 |
| --- | --- |
| 函数 | f(x)=x^2 |
| 离子 | SO_4^{2-} |
结尾中文保持完整)",
    R"(中文x²+y²=1
中文Fe³⁺、SO₄²⁻、NH₄⁺
原子²³⁵U、₉₂U
单位m·s⁻²、v⃗₀
繁體：函數、電荷、溫度
底部中文不得裁切)",
    R"(常见离子与分组
2OH^- + 2H^+ = 2H_2O
\ce{2OH^- + Ca^{2+} -> Ca(OH)2}
\ce{2NH4^+ + SO4^{2-} -> (NH4)2SO4}
\boxed{{a}\in(-\infty,-1]}
\(\{x\in\mathbb{R}\mid x^2\ge 0\}\)
下行中文完整)",
    R"(常用数学物理符号
\alpha\beta\gamma\delta\epsilon\theta\lambda\mu\pi\rho\sigma\phi\psi\omega
\Gamma\Delta\Theta\Lambda\Xi\Pi\Sigma\Phi\Psi\Omega
\mathbb{N}\subset\mathbb{Z}\subset\mathbb{Q}\subset\mathbb{R}\subset\mathbb{C}
\forall x\exists y: x\ne y
\nabla\cdot E=\rho/\epsilon
\partial f/\partial x, \hbar\omega
\oint\iint\iiint\propto\parallel\perp
\lfloor x\rfloor\lceil x\rceil\langle u,v\rangle
\sin x\cos x\ln x\lim_{n}a_n
\ce{H2O <=> H^+ + OH^-}
末行中文完整)",
    "\x03" "# 思考标题\n**思考强调**\n\n思考空行颜色一致\n2OH^-、SO_4^{2-}\n\n" "\x03" "正文颜色保持一致\n下行中文完整",
    R"(差商与小下标
```text
[f(x₁)-f(x₂)] / (x₁-x₂)
=1-x_1-x_2
```
2OH^-、SO_4^{2-}
下行中文保持完整)"
  };
  return page>=1 && page<=sizeof(samples)/sizeof(samples[0])?samples[page-1]:nullptr;
}
