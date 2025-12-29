import bs4
from langchain_community.document_loaders import DirectoryLoader, PyPDFDirectoryLoader, TextLoader
from langchain_ollama import OllamaLLM
from langchain_text_splitters import RecursiveCharacterTextSplitter
from langchain_community.embeddings import OllamaEmbeddings
from langchain_chroma import Chroma
from langchain_community.llms import Ollama
from langchain_core.prompts import ChatPromptTemplate
from langchain_core.runnables import RunnablePassthrough
from langchain_core.output_parsers import StrOutputParser


loader = DirectoryLoader(
    path='./data',
    glob="./*.md",
    loader_cls=TextLoader
)
docs = loader.load()

# Check if documents were loaded
if not docs:
    print("No documents found in the 'data' folder. Please add some .md files.")
    exit()

print(f"Loaded {len(docs)} documents.")

# # 1. Load Data
# # We will load a blog post as our source knowledge
# web_loader = WebBaseLoader(
#     web_paths=("https://lilianweng.github.io/posts/2023-06-23-agent/",),
#     bs_kwargs=dict(
#         parse_only=bs4.SoupStrainer(
#             class_=("post-content", "post-title", "post-header")
#         )
#     ),
# )

print("Loading PDF documents...")
loader_pdf = PyPDFDirectoryLoader(
    path='./data',
    glob="./*.pdf",
)
pdf_docs = loader_pdf.load()
print(f"Loaded {len(pdf_docs)} PDF documents.")



# 2. Split Data
text_splitter = RecursiveCharacterTextSplitter(chunk_size=500, chunk_overlap=200)
splits = text_splitter.split_documents(docs)
splits_web = text_splitter.split_documents(pdf_docs)
splits.extend(splits_web)
print(f"Loaded {len(splits)} documents.")


# 2. Split Data
# Large text needs to be split into smaller chunks for the vector DB

# 3. Create Vector Store (ChromaDB)
# We use 'nomic-embed-text' to convert text chunks into numbers (vectors)
print("Creating vector store...")
vectorstore = Chroma.from_documents(
    documents=splits,
    embedding=OllamaEmbeddings(model="mxbai-embed-large")
)
print(f"Loaded vectors.")

# 4. Create the Retriever
retriever = vectorstore.as_retriever()

# 5. Define the LLM (Llama 3 via Ollama)
llm = OllamaLLM(model="llama3")

# 6. Define the Prompt Template
template = """
Answer the question based only on the following context:
{context}

Question: {question}
"""
prompt = ChatPromptTemplate.from_template(template)

# 7. Build the Chain
# This chain does the following:
# 1. Takes the user question.
# 2. Retrieves relevant context from ChromaDB.
# 3. Formats the context.
# 4. Passes context + question to Llama 3.
def format_docs(docs):
    return "\n\n".join(doc.page_content for doc in docs)

rag_chain = (
    {"context": retriever | format_docs, "question": RunnablePassthrough()}
    | prompt
    | llm
    | StrOutputParser()
)

while True:
# 8. Run the Chain

    question = input("Question: ")
    print(f"Question: {question}\n")
    print("Generating answer...\n")

    response = rag_chain.invoke(question)
    print(response)
