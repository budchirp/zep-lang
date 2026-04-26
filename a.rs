HIRProgram(
  symbols: [
    Symbol(
      name: "Result$1T",
      kind: Type,
      visibility: public,
      type: StructType(
        name: "Result$1T",
        generic_parameters: [],
        fields: [
          StructFieldType(
            name: "value",
            type: IntegerType(
              is_unsigned: false,
              size: 32
            )
          ),
          StructFieldType(
            name: "error",
            type: StructType(
              name: "Error",
              generic_parameters: [],
              fields: [
                StructFieldType(
                  name: "message",
                  type: StringType()
                )
              ]
            )
          )
        ]
      )
    ),
    Symbol(
      name: "Error",
      kind: Type,
      visibility: public,
      type: StructType(
        name: "Error",
        generic_parameters: [],
        fields: [
          StructFieldType(
            name: "message",
            type: StringType()
          )
        ]
      )
    ),
    Symbol(
      name: "Result$3i32",
      kind: Type,
      visibility: public,
      type: StructType(
        name: "Result$3i32",
        generic_parameters: [],
        fields: [
          StructFieldType(
            name: "value",
            type: IntegerType(
              is_unsigned: false,
              size: 32
            )
          ),
          StructFieldType(
            name: "error",
            type: StructType(
              name: "Error",
              generic_parameters: [],
              fields: [
                StructFieldType(
                  name: "message",
                  type: StringType()
                )
              ]
            )
          )
        ]
      )
    ),
    Symbol(
      name: "Vector",
      kind: Type,
      visibility: public,
      type: StructType(
        name: "Vector",
        generic_parameters: [],
        fields: [
          StructFieldType(
            name: "x",
            type: IntegerType(
              is_unsigned: false,
              size: 32
            )
          ),
          StructFieldType(
            name: "y",
            type: IntegerType(
              is_unsigned: false,
              size: 32
            )
          ),
          StructFieldType(
            name: "z",
            type: IntegerType(
              is_unsigned: false,
              size: 32
            )
          )
        ]
      )
    ),
    Symbol(
      name: "add$3i32",
      kind: Function,
      visibility: public,
      type: FunctionType(
        name: "add$3i32",
        return_type: StructType(
          name: "Result$3i32",
          generic_parameters: [],
          fields: [
            StructFieldType(
              name: "value",
              type: IntegerType(
                is_unsigned: false,
                size: 32
              )
            ),
            StructFieldType(
              name: "error",
              type: StructType(
                name: "Error",
                generic_parameters: [],
                fields: [
                  StructFieldType(
                    name: "message",
                    type: StringType()
                  )
                ]
              )
            )
          ]
        ),
        parameters: [
          ParameterType(
            name: "a",
            type: IntegerType(
              is_unsigned: false,
              size: 32
            )
          ),
          ParameterType(
            name: "b",
            type: IntegerType(
              is_unsigned: false,
              size: 32
            )
          )
        ],
        generics: [
          GenericParameterType(
            name: "T",
            constraint: IntegerType(
              is_unsigned: false,
              size: 32
            )
          )
        ],
        variadic: false
      )
    ),
    Symbol(
      name: "allocate$6Vector",
      kind: Function,
      visibility: private,
      type: FunctionType(
        name: "allocate$6Vector",
        return_type: PointerType(
          is_mutable: true,
          element: StructType(
            name: "Vector",
            generic_parameters: [],
            fields: [
              StructFieldType(
                name: "x",
                type: IntegerType(
                  is_unsigned: false,
                  size: 32
                )
              ),
              StructFieldType(
                name: "y",
                type: IntegerType(
                  is_unsigned: false,
                  size: 32
                )
              ),
              StructFieldType(
                name: "z",
                type: IntegerType(
                  is_unsigned: false,
                  size: 32
                )
              )
            ]
          )
        ),
        parameters: [
          ParameterType(
            name: "value",
            type: StructType(
              name: "Vector",
              generic_parameters: [],
              fields: [
                StructFieldType(
                  name: "x",
                  type: IntegerType(
                    is_unsigned: false,
                    size: 32
                  )
                ),
                StructFieldType(
                  name: "y",
                  type: IntegerType(
                    is_unsigned: false,
                    size: 32
                  )
                ),
                StructFieldType(
                  name: "z",
                  type: IntegerType(
                    is_unsigned: false,
                    size: 32
                  )
                )
              ]
            )
          )
        ],
        generics: [
          GenericParameterType(
            name: "T",
            constraint: null
          )
        ],
        variadic: false
      )
    ),
    Symbol(
      name: "main",
      kind: Function,
      visibility: public,
      type: FunctionType(
        name: "main",
        return_type: IntegerType(
          is_unsigned: false,
          size: 32
        ),
        parameters: [],
        generics: [],
        variadic: false
      )
    ),
    Symbol(
      name: "free",
      kind: Function,
      visibility: public,
      type: FunctionType(
        name: "free",
        return_type: VoidType(),
        parameters: [
          ParameterType(
            name: "ptr",
            type: PointerType(
              is_mutable: false,
              element: VoidType()
            )
          )
        ],
        generics: [],
        variadic: false
      )
    ),
    Symbol(
      name: "malloc",
      kind: Function,
      visibility: private,
      type: FunctionType(
        name: "malloc",
        return_type: PointerType(
          is_mutable: false,
          element: VoidType()
        ),
        parameters: [
          ParameterType(
            name: "size",
            type: IntegerType(
              is_unsigned: false,
              size: 64
            )
          )
        ],
        generics: [],
        variadic: false
      )
    ),
    Symbol(
      name: "printf",
      kind: Function,
      visibility: public,
      type: FunctionType(
        name: "printf",
        return_type: VoidType(),
        parameters: [
          ParameterType(
            name: "fmt",
            type: StringType()
          ),
          ParameterType(
            name: "args",
            type: ArrayType(
              element: StringType(),
              size: null
            )
          )
        ],
        generics: [],
        variadic: true
      )
    )
  ]
  statements: [
    FunctionDeclaration(
      name: "allocate$6Vector",
      visibility: private,
      parameters: [
        Parameter(
          name: "value",
          type: StructType(
            name: "Vector",
            generic_parameters: [],
            fields: [
              StructFieldType(
                name: "x",
                type: IntegerType(
                  is_unsigned: false,
                  size: 32
                )
              ),
              StructFieldType(
                name: "y",
                type: IntegerType(
                  is_unsigned: false,
                  size: 32
                )
              ),
              StructFieldType(
                name: "z",
                type: IntegerType(
                  is_unsigned: false,
                  size: 32
                )
              )
            ]
          )
        )
      ],
      return_type: PointerType(
        is_mutable: true,
        element: StructType(
          name: "Vector",
          generic_parameters: [],
          fields: [
            StructFieldType(
              name: "x",
              type: IntegerType(
                is_unsigned: false,
                size: 32
              )
            ),
            StructFieldType(
              name: "y",
              type: IntegerType(
                is_unsigned: false,
                size: 32
              )
            ),
            StructFieldType(
              name: "z",
              type: IntegerType(
                is_unsigned: false,
                size: 32
              )
            )
          ]
        )
      ),
      body: BlockStatement(statements: [
        VarDeclaration(
          name: "ptr",
          storage: var,
          type: PointerType(
            is_mutable: true,
            element: AnyType()
          ),
          initializer: BinaryExpression(
            op: as,
            type: PointerType(
              is_mutable: true,
              element: AnyType()
            ),
            left: CallExpression(
              type: PointerType(
                is_mutable: false,
                element: VoidType()
              ),
              callee: IdentifierExpression(
                name: "malloc",
                type: FunctionType(
                  name: "malloc",
                  return_type: PointerType(
                    is_mutable: false,
                    element: VoidType()
                  ),
                  parameters: [
                    ParameterType(
                      name: "size",
                      type: IntegerType(
                        is_unsigned: false,
                        size: 64
                      )
                    )
                  ],
                  generics: [],
                  variadic: false
                )
              ),
              arguments: [
                BinaryExpression(
                  op: as,
                  type: IntegerType(
                    is_unsigned: false,
                    size: 64
                  ),
                  left: NumberLiteral(
                    value: "123",
                    type: IntegerType(
                      is_unsigned: false,
                      size: 32
                    )
                  ),
                  right: TypeExpression(
                    type: IntegerType(
                      is_unsigned: false,
                      size: 64
                    )
                  )
                )
              ]
            ),
            right: TypeExpression(
              type: PointerType(
                is_mutable: true,
                element: AnyType()
              )
            )
          )
        ),
        ExpressionStatement(
          expression: AssignExpression(
            type: AnyType(),
            target: UnaryExpression(
              op: *,
              type: AnyType(),
              operand: IdentifierExpression(
                name: "ptr",
                type: PointerType(
                  is_mutable: true,
                  element: AnyType()
                )
              )
            ),
            value: IdentifierExpression(
              name: "value",
              type: AnyType()
            )
          )
        ),
        ReturnStatement(
          value: IdentifierExpression(
            name: "ptr",
            type: PointerType(
              is_mutable: true,
              element: AnyType()
            )
          )
        )
      ])
    ),
    FunctionDeclaration(
      name: "add$3i32",
      visibility: public,
      parameters: [
        Parameter(
          name: "a",
          type: IntegerType(
            is_unsigned: false,
            size: 32
          )
        ),
        Parameter(
          name: "b",
          type: IntegerType(
            is_unsigned: false,
            size: 32
          )
        )
      ],
      return_type: StructType(
        name: "Result$3i32",
        generic_parameters: [],
        fields: [
          StructFieldType(
            name: "value",
            type: IntegerType(
              is_unsigned: false,
              size: 32
            )
          ),
          StructFieldType(
            name: "error",
            type: StructType(
              name: "Error",
              generic_parameters: [],
              fields: [
                StructFieldType(
                  name: "message",
                  type: StringType()
                )
              ]
            )
          )
        ]
      ),
      body: BlockStatement(statements: [
        ReturnStatement(
          value: StructLiteralExpression(
            name: "Result$3i32",
            type: StructType(
              name: "Result$3i32",
              generic_parameters: [],
              fields: [
                StructFieldType(
                  name: "value",
                  type: IntegerType(
                    is_unsigned: false,
                    size: 32
                  )
                ),
                StructFieldType(
                  name: "error",
                  type: StructType(
                    name: "Error",
                    generic_parameters: [],
                    fields: [
                      StructFieldType(
                        name: "message",
                        type: StringType()
                      )
                    ]
                  )
                )
              ]
            ),
            fields: [
              Field(
                name: "value",
                value: BinaryExpression(
                  op: +,
                  type: IntegerType(
                    is_unsigned: false,
                    size: 32
                  ),
                  left: IdentifierExpression(
                    name: "a",
                    type: IntegerType(
                      is_unsigned: false,
                      size: 32
                    )
                  ),
                  right: IdentifierExpression(
                    name: "b",
                    type: IntegerType(
                      is_unsigned: false,
                      size: 32
                    )
                  )
                )
              ),
              Field(
                name: "error",
                value: StructLiteralExpression(
                  name: "Error",
                  type: StructType(
                    name: "Error",
                    generic_parameters: [],
                    fields: [
                      StructFieldType(
                        name: "message",
                        type: StringType()
                      )
                    ]
                  ),
                  fields: [
                    Field(
                      name: "message",
                      value: StringLiteral(
                        value: "",
                        type: StringType()
                      )
                    )
                  ]
                )
              )
            ]
          )
        )
      ])
    )
  ],
)
