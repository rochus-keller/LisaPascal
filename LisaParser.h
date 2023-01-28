

// This file was automatically generated by Coco/R; don't modify it.
#if !defined(Lisa_COCO_PARSER_H__)
#define Lisa_COCO_PARSER_H__

#include <QStack>
#include <LisaPascal/LisaSynTree.h>


namespace Lisa {


class PpLexer;
class Parser {
private:
	enum {
		_EOF=0,
		_T_Literals_=1,
		_T_Lpar=2,
		_T_Latt=3,
		_T_Rpar=4,
		_T_Star=5,
		_T_Ratt=6,
		_T_Plus=7,
		_T_Comma=8,
		_T_Minus=9,
		_T_Dot=10,
		_T_2Dot=11,
		_T_Slash=12,
		_T_Colon=13,
		_T_ColonEq=14,
		_T_Semi=15,
		_T_Lt=16,
		_T_Leq=17,
		_T_LtGt=18,
		_T_Eq=19,
		_T_Gt=20,
		_T_Geq=21,
		_T_At=22,
		_T_Lbrack=23,
		_T_Rbrack=24,
		_T_Hat=25,
		_T_Lbrace=26,
		_T_Rbrace=27,
		_T_Keywords_=28,
		_T_and=29,
		_T_array=30,
		_T_begin=31,
		_T_case=32,
		_T_const=33,
		_T_creation=34,
		_T_div=35,
		_T_do=36,
		_T_downto=37,
		_T_else=38,
		_T_end=39,
		_T_external=40,
		_T_file=41,
		_T_for=42,
		_T_forward=43,
		_T_function=44,
		_T_goto=45,
		_T_if=46,
		_T_implementation=47,
		_T_in=48,
		_T_inline=49,
		_T_interface=50,
		_T_intrinsic=51,
		_T_label=52,
		_T_methods=53,
		_T_mod=54,
		_T_nil=55,
		_T_not=56,
		_T_of=57,
		_T_or=58,
		_T_otherwise=59,
		_T_packed=60,
		_T_procedure=61,
		_T_program=62,
		_T_record=63,
		_T_repeat=64,
		_T_set=65,
		_T_string=66,
		_T_subclass=67,
		_T_then=68,
		_T_to=69,
		_T_type=70,
		_T_unit=71,
		_T_until=72,
		_T_uses=73,
		_T_var=74,
		_T_while=75,
		_T_with=76,
		_T_Specials_=77,
		_T_identifier=78,
		_T_unsigned_real=79,
		_T_digit_sequence=80,
		_T_hex_digit_sequence=81,
		_T_string_literal=82,
		_T_Comment=83,
		_T_Directive=84,
		_T_Eof=85,
		_T_MaxToken_=86
	};
	int maxT;

	int errDist;
	int minErrDist;

	void SynErr(int n, const char* ctx = 0);
	void Get();
	void Expect(int n, const char* ctx = 0);
	bool StartOf(int s);
	void ExpectWeak(int n, int follow);
	bool WeakSeparator(int n, int syFol, int repFol);
    void SynErr(int line, int col, int n, const char* ctx, const QString&, const QString& path );

public:
	PpLexer *scanner;
	struct Error
	{
		QString msg;
		int row, col;
		QString path;
	};
	QList<Error> errors;
	
	void error(int row, int col, const QString& msg, const QString& path)
	{
		Error e;
		e.row = row;
		e.col = col;
		e.msg = msg;
		e.path = path;
		errors.append(e);
	}

	Token d_cur;
	Token d_next;
	QList<Token> d_comments;
	struct TokDummy
	{
		int kind;
	};
	TokDummy d_dummy;
	TokDummy *la;			// lookahead token
	
	int peek( quint8 la = 1 );

    void RunParser();

    
Lisa::SynTree d_root;
	QStack<Lisa::SynTree*> d_stack;
	void addTerminal() {
		if( d_cur.d_type != Lisa::Tok_Semi && d_cur.d_type != Lisa::Tok_Comma && d_cur.d_type != Lisa::Tok_Dot ){
			Lisa::SynTree* n = new Lisa::SynTree( d_cur ); d_stack.top()->d_children.append(n);
		}
	}



	Parser(PpLexer *scanner);
	~Parser();
	void SemErr(const char* msg);

	void LisaPascal();
	void program_();
	void regular_unit();
	void non_regular_unit();
	void program_heading();
	void uses_clause();
	void block();
	void statement_part();
	void program_parameters();
	void identifier_list();
	void unit_heading();
	void interface_part();
	void implementation_part();
	void constant_declaration_part();
	void type_declaration_part();
	void variable_declaration_part();
	void procedure_and_function_interface_part();
	void procedure_and_function_declaration_part();
	void label_declaration_part();
	void label_();
	void constant_declaration();
	void constant();
	void sign();
	void constant_identifier();
	void unsigned_number();
	void type_declaration();
	void type_();
	void variable_declaration();
	void procedure_heading();
	void function_heading();
	void procedure_declaration();
	void function_declaration();
	void body_();
	void compound_statement();
	void formal_parameter_list();
	void result_type();
	void type_identifier();
	void formal_parameter_section();
	void parameter_declaration();
	void statement_sequence();
	void statement();
	void simple_statement();
	void structured_statement();
	void assigOrCall();
	void goto_statement();
	void variable_reference();
	void expression();
	void actual_parameter_list();
	void repetitive_statement();
	void conditional_statement();
	void with_statement();
	void while_statement();
	void repeat_statement();
	void for_statement();
	void variable_identifier();
	void initial_value();
	void final_value();
	void if_statement();
	void case_statement();
	void case_limb();
	void otherwise_clause();
	void case_label_list();
	void actual_parameter();
	void simple_expression();
	void relational_operator();
	void term();
	void addition_operator();
	void factor();
	void multiplication_operator();
	void qualifier();
	void set_literal();
	void index();
	void field_designator();
	void dereferencer();
	void expression_list();
	void field_identifier();
	void member_group();
	void simple_type();
	void string_type();
	void structured_type();
	void pointer_type();
	void subrange_type();
	void enumerated_type();
	void ordinal_type();
	void size_attribute();
	void unsigned_integer();
	void array_type();
	void record_type();
	void set_type();
	void file_type();
	void index_type();
	void field_list();
	void fixed_part();
	void variant_part();
	void field_declaration();
	void tag_field();
	void variant();

	void Parse();

}; // end Parser

} // namespace


#endif

