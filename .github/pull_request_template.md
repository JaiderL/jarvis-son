## Descripción

<!-- ¿Qué cambia y por qué? Enlace el issue: "Cierra #N" -->

## Tipo de cambio

- [ ] Nueva funcionalidad
- [ ] Corrección de error
- [ ] Documentación
- [ ] Scripts / CI

## Pasos de prueba

<!-- Comandos exactos para reproducir la prueba, uno por línea -->

```bash
```

## Evidencia

<!-- Salida de la CI, capturas de Termux, extracto de logs/metricas.csv (sin audio ni texto de la conversación) -->

| Verificación | Dónde | Resultado |
|---|---|---|
| Compila y pasan las pruebas | CI (Linux) | |
| Formato clang-format | CI | |
| Probado en el teléfono | Termux (modelo de teléfono, RAM) | |

## Limitaciones

<!-- Qué no se probó, riesgos conocidos, qué falta validar en el teléfono real -->

## Lista de control

- [ ] No uso `system()`, `popen()` ni concateno texto del usuario en comandos de shell
- [ ] Reviso el código de salida de cada proceso y capturo stdout/stderr
- [ ] No subo pesos de modelos, audio ni texto de conversaciones
- [ ] Código, comentarios y mensajes en español
- [ ] La CI pasa
