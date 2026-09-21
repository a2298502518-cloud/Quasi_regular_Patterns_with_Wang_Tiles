"use strict";

const fs = require("fs");
const path = require("path");
const { mathjax } = require("mathjax-full/js/mathjax.js");
const { TeX } = require("mathjax-full/js/input/tex.js");
const { SVG } = require("mathjax-full/js/output/svg.js");
const { liteAdaptor } = require("mathjax-full/js/adaptors/liteAdaptor.js");
const { RegisterHTMLHandler } = require("mathjax-full/js/handlers/html.js");
const { AllPackages } = require("mathjax-full/js/input/tex/AllPackages.js");
const { AssistiveMmlHandler } = require("mathjax-full/js/a11y/assistive-mml.js");

const outputDir = path.join(__dirname, "equations");
fs.mkdirSync(outputDir, { recursive: true });

const equations = [
  ["eq01", String.raw`x=g+\xi,\qquad \xi=(u,v)\in[0,1]^2,\qquad \tau=(c_S,c_N,c_W,c_E)`],
  ["eq02", String.raw`c_E(i,j)=c_W(i+1,j),\qquad c_N(i,j)=c_S(i,j+1)`],
  ["eq03", String.raw`\vartheta_c=\frac{2\pi c}{K},\qquad b(r)=16r^2(1-r)^2`],
  ["eq04", String.raw`\begin{aligned}a_c(r)&=w_*+\rho\cos\vartheta_c\,b(r), & d_c(r)&=\sigma\sin\vartheta_c\,b(r),\\ \widehat a_c(r)&=a_c(r)-w_*&&\end{aligned}`],
  ["eq05", String.raw`b(0)=b(1)=b'(0)=b'(1)=0,\qquad W=w_*,\quad \nabla W=(0,0)`],
  ["eq06", String.raw`\begin{aligned} h_{00}(r)&=2r^3-3r^2+1, & h_{10}(r)&=r^3-2r^2+r,\\ h_{01}(r)&=-2r^3+3r^2, & h_{11}(r)&=r^3-r^2. \end{aligned}`],
  ["eq07", String.raw`\begin{aligned} P_y(u,v)={}&h_{00}(v)\widehat a_{c_S}(u)+h_{10}(v)d_{c_S}(u)\\ &+h_{01}(v)\widehat a_{c_N}(u)+h_{11}(v)d_{c_N}(u) \end{aligned}`],
  ["eq08", String.raw`\begin{aligned} P_x(u,v)={}&h_{00}(u)\widehat a_{c_W}(v)+h_{10}(u)d_{c_W}(v)\\ &+h_{01}(u)\widehat a_{c_E}(v)+h_{11}(u)d_{c_E}(v) \end{aligned}`],
  ["eq09", String.raw`w_\tau(u,v)=w_*+P_x(u,v)+P_y(u,v)`],
  ["eq10", String.raw`\left|w_\tau-w_*\right|\le 2|\rho|+\frac{|\sigma|}{2}\le\min(w_*,1-w_*)`],
  ["eq11", String.raw`\theta_r=\frac{2\pi r}{q},\qquad e_r=\begin{pmatrix}\cos\theta_r\\ \sin\theta_r\end{pmatrix},\qquad r=0,\ldots,q-1`],
  ["eq12", String.raw`Q_{q,\kappa}(x)=\frac1q\sum_{r=0}^{q-1}\cos\!\left(\kappa e_r^{\mathsf T}x\right)`],
  ["eq13", String.raw`\begin{aligned}p_x\cos\theta_r+p_y\sin\theta_r&=p^{\mathsf T}e_r,\\ \kappa e_r^{\mathsf T}x+p^{\mathsf T}e_r&=\kappa e_r^{\mathsf T}\!\left(x+\frac{p}{\kappa}\right)\end{aligned}`],
  ["eq14", String.raw`c_r=\cos(2\theta_r),\qquad s_r=\sin(2\theta_r),\qquad U(x)=\frac{W(x)-w_*}{R_W}`],
  ["eq15", String.raw`A(x)=A_0+\lambda_AU(x),\qquad B(x)=B_0+\lambda_BU(x)`],
  ["eq16", String.raw`C_\Theta(x)=\frac1q\sum_{r=0}^{q-1}\cos\!\left[\kappa e_r^{\mathsf T}x+A(x)c_r+B(x)s_r\right]`],
  ["eq17", String.raw`F(x)=C(x)+\beta_m\!\left(1-C(x)^2\right)D_m(x)+\beta_f\!\left(1-C(x)^2\right)^2D_f(x)`],
  ["eq18", String.raw`\begin{aligned}\chi_r(x)&=\kappa e_r^{\mathsf T}x+A(x)c_r+B(x)s_r,\\ \nabla\chi_r&=\kappa e_r+c_r\nabla A+s_r\nabla B\end{aligned}`],
  ["eq19", String.raw`\begin{aligned}\nabla C_\Theta(x)=-\frac1q\sum_{r=0}^{q-1}\sin\chi_r(x)\,\bigl[&\kappa e_r+c_r\nabla A(x)\\ &+s_r\nabla B(x)\bigr]\end{aligned}`],
  ["eq20", String.raw`\nabla A=\frac{\lambda_A}{R_W}\nabla W,\qquad \nabla B=\frac{\lambda_B}{R_W}\nabla W`],
  ["eq21", String.raw`|C_\Theta|\le1,\qquad |D_m|,|D_f|\le\sqrt2,\qquad 2\sqrt2\,(\beta_m+\beta_f)\le1`],
  ["eq22", String.raw`E_{\mathrm{RMS}}(i,j)=\sqrt{\frac{1}{N}\sum_{p=1}^{N}\left[C_i(x_p)-C_j(x_p)\right]^2}`],
  ["eq23", String.raw`E_{\mathrm{sign}}(i,j)=\frac{1}{N}\sum_{p=1}^{N}\mathbf{1}\!\left[\left(C_i(x_p)<0\right)\ne\left(C_j(x_p)<0\right)\right]`],
];

const adaptor = liteAdaptor();
AssistiveMmlHandler(RegisterHTMLHandler(adaptor));
const tex = new TeX({ packages: AllPackages });
const svgOutput = new SVG({ fontCache: "local" });
const document = mathjax.document("", { InputJax: tex, OutputJax: svgOutput });

function renderEquation(id, latex) {
  const container = document.convert(latex, { display: true });
  const html = adaptor.outerHTML(container);
  const mathStart = html.indexOf("<math");
  const mathEnd = html.indexOf("</math>");
  if (mathStart < 0 || mathEnd < mathStart) {
    throw new Error(`MathML output is missing for ${id}`);
  }
  const mathml = html.slice(mathStart, mathEnd + "</math>".length);
  fs.writeFileSync(path.join(outputDir, `${id}.mml`), `${mathml}\n`, "utf8");
}

try {
  equations.forEach(([id, latex]) => renderEquation(id, latex));
  fs.writeFileSync(
    path.join(outputDir, "manifest.json"),
    JSON.stringify(equations.map(([id, latex]) => ({ id, latex })), null, 2),
    "utf8",
  );
} catch (error) {
  process.stderr.write(`${error.stack || error}\n`);
  process.exit(1);
}
