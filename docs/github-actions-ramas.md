# OpenCode en GitHub Actions: trabajar sobre una rama distinta a `main`

## Qué hace upstream (y por qué estaba limitado a `main`)

El action `anomalyco/opencode/github` ejecuta `opencode github run`. En el código
fuente (`packages/opencode/src/cli/cmd/github.handler.ts` en v1.19.3, y
`github/index.ts` en `dev`), el flujo para un issue es:

1. `git checkout -b opencode/issue<N>-<timestamp>` — la rama de trabajo sale de
   **el HEAD que dejó el checkout del workflow**.
2. El agente trabaja y hace el commit.
3. `git push -u origin opencode/issue<N>-<timestamp>`.
4. `createPR(repo.default_branch, branch, ...)` — el Pull Request se crea con
   base = **la rama por defecto del repositorio**, tomada de
   `GET /repos/{repo}` → `.default_branch`.

Ese `base` está **hardcodeado**: no hay input del action, variable de entorno
ni flag de CLI que lo cambie. Las únicas variables que lee `opencode github run`
son `MODEL`, `PROMPT`, `AGENT`, `SHARE`, `MENTIONS`, `VARIANT`,
`USE_GITHUB_TOKEN`, `OIDC_BASE_URL`, `GITHUB_TOKEN` y `GITHUB_RUN_ID`; el
comando `github run` solo acepta `--event` y `--token`.

Por eso el PR siempre apuntaba a `main`, aunque el checkout fuera de otra
rama: el *contenido* podía venir de otro lado, pero el *destino* del merge no.

## Qué hace este workflow

El workflow resuelve la rama que pidió el usuario y la usa en los tres puntos
que importan:

| Paso | Qué hace |
|---|---|
| `Resolver rama base solicitada` | Detecta la rama pedida y la valida contra las ramas reales del remoto (`gh api repos/{repo}/branches`). |
| `Checkout del código del repositorio` | Hace checkout de esa rama (`ref:`), de modo que la rama `opencode/issueN-<ts>` del agente **sale de ella** y el diff del PR contiene solo los cambios del agente. |
| `Preparar ref local de la rama por defecto` | Crea el ref local de la rama por defecto, para que la validación de commits que hace el action (`git rev-list <default>..<rama>`) no caiga en su fallback con `fetch --depth=1`. |
| `Resolver Prompt …` | Le inyecta al agente el contexto de rama (qué rama es la base, que no se mueva de rama). |
| `Apuntar el Pull Request a la rama base solicitada` | Reapunta el PR creado por el action con `gh pr edit --base` (equivale a `PATCH /repos/{repo}/pulls/{n}`). |

### Cómo pedir una rama

En el issue o en el comentario:

```
/oc arregla el audio en la rama feature/nova-android-spike
/oc base: feature/nova-android-spike arregla el audio
/oc work on branch feature/nova-android-spike
```

También sirve con la rama mencionada de forma suelta (sin la palabra "rama"),
siempre que el nombre coincida con una rama real del remoto.

Formatos aceptados para la detección explícita: `rama X`, `rama: X`, `base: X`,
`branch X`, `basado en la rama X`.

### Reglas de resolución

1. La rama indicada debe existir en el remoto; si no existe, se usa la rama por
   defecto y se avisa en el prompt (no se rompe el run).
2. Entre varias ramas mencionadas gana la **más larga** (evita que
   `feature/a` pise a `feature/a-b`).
3. Si no se menciona ninguna rama, se usa la rama por defecto del repositorio
   (comportamiento original).
4. Si no se puede consultar el remoto (token o red), no se fuerza ninguna rama:
   el checkout y el PR quedan en la rama por defecto.

### Limitaciones conocidas

- En el flujo de **issue** el PR se crea primero contra la rama por defecto y
  se reapunta después. Entre esos dos instantes el PR existe apuntando a
  `main` (duración: segundos).
- En el flujo de **Pull Request** (comentar sobre un PR existente) el action
  hace checkout de la rama *head* de ese PR y le hace push; la base del PR
  queda como la eligió quien lo abrió. El workflow no la modifica a propósito.
- El reapuntado no se aplica si el agente creó su propia rama y su propio PR
  (upstream detecta el cambio de rama y se salta su push/PR). En ese caso el
  PR lo hizo el agente y hay que verificar su base a mano.
- Si el agente no produce cambios, no hay PR y no hay nada que reapuntar.

## Alternativa: que el agente gestione su propio PR

Upstream también soporta que el agente cree su propia rama y su propio PR: si
el agente cambia de rama durante la sesión, el action lo detecta y se salta su
push y su creación de PR. Con `GITHUB_TOKEN` (o `PAT_TOKEN`) en el entorno, el
agente puede hacer:

```bash
git checkout -b opencode/issue12-fix origin/feature/nova-android-spike
git push -u origin HEAD
gh pr create --base feature/nova-android-spike --title "..." --body "..."
```

Es más explícito, pero depende de que el modelo cumpla la instrucción; el
mecanismo del workflow es determinista.
